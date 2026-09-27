/*
 * audio_buffer.c - Application-level audio ring buffer (playback path).
 *
 * Lock-free (single producer / single consumer), ISR-safe. The producer is the
 * USB audio layer (tud_audio_rx_done / audio_task), the consumer will be the
 * SAI/DMA output (Phase 3). Release/acquire publishes completed copies.
 */
#include "audio_buffer.h"
#include <string.h>
#include <stdatomic.h>

// 64-byte aligned storage in AXI SRAM (D1); Cortex-M7 cache lines are 32 B.
__attribute__((section(".ram_audio_buf"), aligned(64)))
static uint8_t  s_buf[AUDIO_BUFFER_DEPTH];

// Keep an extra lap to distinguish full from empty. Modulo 2*depth also
// supports this non-power-of-two capacity without uint32_t rollover issues.
#define INDEX_MODULUS (2U * AUDIO_BUFFER_DEPTH)
static _Atomic uint32_t s_head;
static _Atomic uint32_t s_tail;

static audio_buffer_stats_t s_stats;

void audio_buffer_init(void) {
  s_head = 0;
  s_tail = 0;
  memset((void*)&s_stats, 0, sizeof(s_stats));
}

void audio_buffer_push(uint8_t const *data, uint32_t n_bytes) {
  // TinyUSB supplies a byte stream, not fixed-size application packets.
  if (n_bytes == 0) {
    return;
  }
  uint32_t h = atomic_load_explicit(&s_head, memory_order_relaxed);
  uint32_t t = atomic_load_explicit(&s_tail, memory_order_acquire);
  uint32_t used = (h + INDEX_MODULUS - t) % INDEX_MODULUS;
  uint32_t free_space = AUDIO_BUFFER_DEPTH - used;

  if (n_bytes > free_space) {
    // Overrun: drop the incoming data, count it.
    s_stats.push_overruns++;
    return;
  }

  // Wrap-around-aware copy.
  uint32_t pos = h % AUDIO_BUFFER_DEPTH;
  uint32_t first_chunk = (pos + n_bytes) <= AUDIO_BUFFER_DEPTH ? n_bytes : (AUDIO_BUFFER_DEPTH - pos);
  memcpy((void*)(s_buf + pos), data, first_chunk);
  memcpy((void*)(s_buf + 0), data + first_chunk, n_bytes - first_chunk);

  s_stats.rx_packets++;
  s_stats.rx_bytes += n_bytes;
  atomic_store_explicit(&s_head, (h + n_bytes) % INDEX_MODULUS, memory_order_release);
}

uint32_t audio_buffer_pop(uint8_t *data, uint32_t n_bytes) {
  uint32_t t = atomic_load_explicit(&s_tail, memory_order_relaxed);
  uint32_t h = atomic_load_explicit(&s_head, memory_order_acquire);
  uint32_t avail = (h + INDEX_MODULUS - t) % INDEX_MODULUS;

  if (n_bytes > avail) {
    s_stats.pop_underruns++;
    n_bytes = avail;
  }
  if (n_bytes == 0) {
    return 0;
  }

  uint32_t pos = t % AUDIO_BUFFER_DEPTH;
  uint32_t first_chunk = (pos + n_bytes) <= AUDIO_BUFFER_DEPTH ? n_bytes : (AUDIO_BUFFER_DEPTH - pos);
  memcpy(data, (void*)(s_buf + pos), first_chunk);
  memcpy(data + first_chunk, (void*)(s_buf + 0), n_bytes - first_chunk);

  atomic_store_explicit(&s_tail, (t + n_bytes) % INDEX_MODULUS, memory_order_release);
  return n_bytes;
}

uint32_t audio_buffer_available(void) {
  uint32_t t = atomic_load_explicit(&s_tail, memory_order_relaxed);
  uint32_t h = atomic_load_explicit(&s_head, memory_order_acquire);
  return (h + INDEX_MODULUS - t) % INDEX_MODULUS;
}

audio_buffer_stats_t const *audio_buffer_stats(void) {
  return &s_stats;
}
