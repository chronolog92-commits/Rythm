#include "audio_buffer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  uint8_t input[AUDIO_BUFFER_DEPTH], output[AUDIO_BUFFER_DEPTH];
  for (unsigned i = 0; i < sizeof(input); ++i) input[i] = (uint8_t)(i * 37U);

  audio_buffer_init();
  audio_buffer_push(input, sizeof(input));
  assert(audio_buffer_available() == sizeof(input));
  audio_buffer_push(input, 32);
  assert(audio_buffer_stats()->push_overruns == 1);
  assert(audio_buffer_pop(output, sizeof(output)) == sizeof(output));
  assert(memcmp(input, output, sizeof(input)) == 0);
  assert(audio_buffer_available() == 0);

  /* Real USB-sized chunks, including 44.1 kHz stereo 16-bit (176/180 B).
   * Repeated cycles cross both storage and index wrap boundaries. */
  unsigned sizes[] = {192, 176, 180, 196};
  audio_buffer_init();
  for (unsigned i = 0; i < 200; ++i) {
    unsigned n = sizes[i % 4];
    audio_buffer_push(input, n);
    assert(audio_buffer_available() == n);
    assert(audio_buffer_pop(output, 7) == 7);
    assert(audio_buffer_pop(output + 7, n - 7) == n - 7);
    assert(memcmp(input, output, n) == 0);
    assert(audio_buffer_available() == 0);
  }
  assert(audio_buffer_stats()->push_overruns == 0);
  assert(audio_buffer_pop(output, 1) == 0);
  assert(audio_buffer_stats()->pop_underruns == 1);
  audio_buffer_push(input, AUDIO_BUFFER_DEPTH + 1);
  assert(audio_buffer_available() == 0);
  assert(audio_buffer_stats()->push_overruns == 1);
  puts("PASS: full/empty, drop-on-full, byte order, wrap, USB chunk sizes, underrun");
  return 0;
}
