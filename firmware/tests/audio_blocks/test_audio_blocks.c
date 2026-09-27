#include "audio_blocks.h"
#include "audio_buffer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t pcm[AUDIO_BLOCK_FRAMES * AUDIO_BLOCK_CHANNELS * 2];

static void make_pcm(int left, int right) {
  for (unsigned f = 0; f < AUDIO_BLOCK_FRAMES; ++f) {
    pcm[4*f] = (uint8_t)left;
    pcm[4*f+1] = (uint8_t)((unsigned)left >> 8);
    pcm[4*f+2] = (uint8_t)right;
    pcm[4*f+3] = (uint8_t)((unsigned)right >> 8);
  }
}

static void feed(const uint8_t *p, unsigned n) {
  audio_buffer_push(p, n);
  audio_blocks_process();
}

static void check_block(const audio_block_t *b, int left, int right) {
  assert(b != NULL);
  for (unsigned f = 0; f < AUDIO_BLOCK_FRAMES; ++f) {
    assert(b->samples[f][0] == (float)left / 32768.0f);
    assert(b->samples[f][1] == (float)right / 32768.0f);
  }
}

int main(void) {
  /* Every byte split, including inside signed samples and stereo frames. */
  make_pcm(-32768, 32767);
  for (unsigned split = 1; split < sizeof(pcm); ++split) {
    audio_blocks_reset(48000);
    feed(pcm, split);
    assert(audio_blocks_acquire() == NULL);
    feed(pcm + split, sizeof(pcm) - split);
    check_block(audio_blocks_acquire(), -32768, 32767);
    assert(g_audio_block_report.pcm_frames == 48);
    assert(g_audio_block_report.input_bytes == sizeof(pcm));
    assert(g_audio_block_report.blocks_published == 1);
    audio_blocks_release();
    assert(audio_blocks_acquire() == NULL);
  }

  /* Distinct frames detect reordering/duplication across varying chunk sizes. */
  uint8_t stream[192 * 8];
  for (unsigned i = 0; i < sizeof(stream)/2; ++i) {
    stream[2*i] = (uint8_t)i;
    stream[2*i+1] = (uint8_t)(i >> 8);
  }
  unsigned chunks[] = {176, 180, 192, 196, 1, 3, 7};
  audio_blocks_reset(44100);
  unsigned pos = 0, chunk = 0, sample = 0;
  while (pos < sizeof(stream)) {
    unsigned n = chunks[chunk++ % 7];
    if (n > sizeof(stream) - pos) n = sizeof(stream) - pos;
    feed(stream + pos, n);
    pos += n;
    const audio_block_t *b;
    while ((b = audio_blocks_acquire()) != NULL) {
      for (unsigned f = 0; f < 48; ++f)
        for (unsigned c = 0; c < 2; ++c)
          assert(b->samples[f][c] == (float)sample++ / 32768.0f);
      audio_blocks_release();
    }
  }
  assert(sample == sizeof(stream)/2);
  assert(g_audio_block_report.blocks_dropped == 0);

  /* A borrowed oldest block is immutable even when the producer overruns. */
  audio_blocks_reset(48000);
  for (int i = 0; i < 4; ++i) { make_pcm(i, -i); feed(pcm, sizeof(pcm)); }
  const audio_block_t *held = audio_blocks_acquire();
  assert(held != NULL);
  for (int i = 4; i < 10; ++i) { make_pcm(i, -i); feed(pcm, sizeof(pcm)); }
  assert(audio_blocks_acquire() == held);
  check_block(held, 0, 0);
  assert(g_audio_block_report.blocks_completed == 10);
  assert(g_audio_block_report.blocks_published == 4);
  assert(g_audio_block_report.blocks_dropped == 6);
  assert(audio_blocks_ready() == 4);
  for (int i = 0; i < 4; ++i) {
    check_block(audio_blocks_acquire(), i, -i);
    audio_blocks_release();
  }
  audio_blocks_release(); /* empty release must not advance consumer */
  assert(audio_blocks_ready() == 0);
  assert(g_audio_block_report.blocks_released == 4);
  for (int i = 0; i < 100; ++i) {
    make_pcm(-16384, 16384); feed(pcm, sizeof(pcm));
    check_block(audio_blocks_acquire(), -16384, 16384);
    audio_blocks_release();
  }

  /* Reset drops partial bytes/frames, queued blocks AND unread byte-ring data. */
  make_pcm(-1, 1); feed(pcm, sizeof(pcm)); feed(pcm, 7);
  audio_buffer_push(pcm, 13);
  uint32_t epoch = g_audio_block_report.epoch;
  audio_blocks_reset(44100);
  assert(g_audio_block_report.epoch == epoch + 1);
  assert(g_audio_block_report.sample_rate == 44100);
  assert(g_audio_block_report.partial_bytes == 0);
  assert(g_audio_block_report.partial_frames == 0);
  assert(audio_blocks_acquire() == NULL);
  assert(audio_buffer_available() == 0);
  make_pcm(-1, 1); feed(pcm, sizeof(pcm));
  check_block(audio_blocks_acquire(), -1, 1);
  assert(g_audio_block_report.first_pcm[0] == -1);
  assert(g_audio_block_report.first_pcm[1] == 1);
  puts("PASS: PCM scaling/LR, all split points, USB chunks, ownership, queue overflow/wrap, reset");
  return 0;
}
