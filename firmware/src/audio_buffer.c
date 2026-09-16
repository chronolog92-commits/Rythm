/*
 * audio_buffer.c - Application-level audio ring buffer (playback path).
 *
 * Lock-free (single producer / single consumer), ISR-safe. The producer is the
 * USB audio layer (tud_audio_rx_done / audio_task), the consumer will be the
 * SAI/DMA output (Phase 3). All pointers are 64-byte (cache-line) aligned.
 */
#include "audio_buffer.h"
#include <string.h>

// 64-byte aligned storage in AXI SRAM (D1). 64 = Cortex-M7 cache line size.
__attribute__((section(".ram_audio_buf"), aligned(64)))
static uint8_t  s_buf[AUDIO_BUFFER_DEPTH];

static volatile uint32_t s_head;   // write index (bytes)
static volatile uint32_t s_tail;   // read index (bytes)

static audio_buffer_stats_t s_stats;

void audio_buffer_init(void) {
  s_head = 0;
  s_tail = 0;
  memset((void*)&s_stats, 0, sizeof(s_stats));
}

void audio_buffer_push(uint8_t const *data, uint32_t n_bytes) {
  // Only accept whole packets for Phase 1 determinism.
  if (n_bytes == 0) {
    return;
  }
  if ((n_bytes % AUDIO_BUFFER_PACKET_SIZE) != 0) {
    s_stats.push_overruns++;
    return;
  }

  uint32_t h = s_head;
  uint32_t t = s_tail;
  uint32_t free_space = AUDIO_BUFFER_DEPTH - ((h - t) & (AUDIO_BUFFER_DEPTH - 1));

  if (n_bytes > free_space) {
    // Overrun: drop the incoming data, count it.
    s_stats.push_overruns++;
    return;
  }

  // Wrap-around-aware copy.
  uint32_t first_chunk = (h + n_bytes) <= AUDIO_BUFFER_DEPTH ? n_bytes : (AUDIO_BUFFER_DEPTH - h);
  memcpy((void*)(s_buf + h), data, first_chunk);
  memcpy((void*)(s_buf + 0), data + first_chunk, n_bytes - first_chunk);

  s_stats.rx_packets++;
  s_stats.rx_bytes += n_bytes;
  s_head = (h + n_bytes) & (AUDIO_BUFFER_DEPTH - 1);
}

uint32_t audio_buffer_pop(uint8_t *data, uint32_t n_bytes) {
  uint32_t t = s_tail;
  uint32_t h = s_head;
  uint32_t avail = (h - t) & (AUDIO_BUFFER_DEPTH - 1);

  if (n_bytes > avail) {
    s_stats.pop_underruns++;
    n_bytes = avail;
  }
  if (n_bytes == 0) {
    return 0;
  }

  uint32_t first_chunk = (t + n_bytes) <= AUDIO_BUFFER_DEPTH ? n_bytes : (AUDIO_BUFFER_DEPTH - t);
  memcpy(data, (void*)(s_buf + t), first_chunk);
  memcpy(data + first_chunk, (void*)(s_buf + 0), n_bytes - first_chunk);

  s_tail = (t + n_bytes) & (AUDIO_BUFFER_DEPTH - 1);
  return n_bytes;
}

uint32_t audio_buffer_available(void) {
  return (s_head - s_tail) & (AUDIO_BUFFER_DEPTH - 1);
}

audio_buffer_stats_t const *audio_buffer_stats(void) {
  return &s_stats;
}
