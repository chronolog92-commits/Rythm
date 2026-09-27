#include "audio_blocks.h"
#include "audio_buffer.h"
#include <stdatomic.h>
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(float) == 4, "Float32 required");
_Static_assert(sizeof(audio_block_t) == 384, "48 stereo Float32 frames");
_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "ISR consumer requires lock-free indices");

/* Linker + MPU region 1 provide noncacheable normal AXI SRAM. */
__attribute__((section(".audio_blocks"), aligned(32)))
audio_block_t g_audio_blocks[AUDIO_BLOCK_QUEUE_SIZE];
__attribute__((section(".audio_blocks"), aligned(32)))
static audio_block_t s_staging;

volatile audio_block_report_t g_audio_block_report;
static _Atomic uint32_t s_write;
static _Atomic uint32_t s_read;
static uint8_t s_frame[4];
static uint32_t s_frame_bytes;
static uint32_t s_frames;

void audio_blocks_reset(uint32_t sample_rate) {
  uint32_t epoch = g_audio_block_report.epoch + 1;
  audio_buffer_init();
  atomic_store_explicit(&s_write, 0, memory_order_relaxed);
  atomic_store_explicit(&s_read, 0, memory_order_relaxed);
  s_frame_bytes = 0;
  s_frames = 0;
  memset(s_frame, 0, sizeof(s_frame));
  memset(g_audio_blocks, 0, sizeof(g_audio_blocks));
  memset(&s_staging, 0, sizeof(s_staging));
  g_audio_block_report = (audio_block_report_t){
    .magic = 0x41424c4bU, /* ABLK */
    .version = 1,
    .epoch = epoch,
    .sample_rate = sample_rate,
  };
}

static void publish_block(void) {
  g_audio_block_report.blocks_completed++;
  uint32_t w = atomic_load_explicit(&s_write, memory_order_relaxed);
  uint32_t r = atomic_load_explicit(&s_read, memory_order_acquire);
  if ((uint32_t)(w - r) == AUDIO_BLOCK_QUEUE_SIZE) {
    g_audio_block_report.blocks_dropped++;
    return;
  }
  g_audio_blocks[w % AUDIO_BLOCK_QUEUE_SIZE] = s_staging;
  g_audio_block_report.blocks_published++;
  atomic_store_explicit(&s_write, w + 1, memory_order_release);
}

static void accept_byte(uint8_t byte) {
  s_frame[s_frame_bytes++] = byte;
  if (s_frame_bytes != sizeof(s_frame)) return;
  s_frame_bytes = 0;
  for (unsigned channel = 0; channel < AUDIO_BLOCK_CHANNELS; ++channel) {
    uint32_t raw = (uint32_t)s_frame[2 * channel] |
                   ((uint32_t)s_frame[2 * channel + 1] << 8);
    /* Explicit sign extension avoids implementation-defined uint16->int16. */
    int32_t value = raw >= 0x8000U ? (int32_t)raw - 65536 : (int32_t)raw;
    s_staging.samples[s_frames][channel] = (float)value * (1.0f / 32768.0f);
    if (value != 0) g_audio_block_report.nonzero_samples++;
    if (g_audio_block_report.blocks_completed == 0)
      g_audio_block_report.first_pcm[2 * s_frames + channel] = value;
  }
  g_audio_block_report.pcm_frames++;
  if (++s_frames == AUDIO_BLOCK_FRAMES) {
    publish_block();
    s_frames = 0;
  }
}

void audio_blocks_process(void) {
  uint8_t bytes[AUDIO_BLOCK_FRAMES * AUDIO_BLOCK_CHANNELS * 2];
  uint32_t available;
  while ((available = audio_buffer_available()) != 0) {
    uint32_t n = available < sizeof(bytes) ? available : sizeof(bytes);
    n = audio_buffer_pop(bytes, n);
    if (n == 0) break;
    g_audio_block_report.input_bytes += n;
    for (uint32_t i = 0; i < n; ++i) accept_byte(bytes[i]);
  }
  g_audio_block_report.partial_bytes = s_frame_bytes;
  g_audio_block_report.partial_frames = s_frames;
}

const audio_block_t *audio_blocks_acquire(void) {
  uint32_t r = atomic_load_explicit(&s_read, memory_order_relaxed);
  uint32_t w = atomic_load_explicit(&s_write, memory_order_acquire);
  return w == r ? NULL : &g_audio_blocks[r % AUDIO_BLOCK_QUEUE_SIZE];
}

void audio_blocks_release(void) {
  uint32_t r = atomic_load_explicit(&s_read, memory_order_relaxed);
  uint32_t w = atomic_load_explicit(&s_write, memory_order_acquire);
  if (w == r) return;
  g_audio_block_report.blocks_released++;
  atomic_store_explicit(&s_read, r + 1, memory_order_release);
}

uint32_t audio_blocks_ready(void) {
  uint32_t r = atomic_load_explicit(&s_read, memory_order_relaxed);
  uint32_t w = atomic_load_explicit(&s_write, memory_order_acquire);
  return (uint32_t)(w - r);
}
