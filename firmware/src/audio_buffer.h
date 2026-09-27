/*
 * audio_buffer.h - Application-level audio ring buffer (playback path, Phase 1).
 *
 * The TinyUSB audio driver receives isochronous packets into its internal
 * software FIFO (cache-coherency handled by the DWC2 port). This ring buffer
 * is the application-level buffer: audio is read out of the TinyUSB FIFO into
 * this ring buffer by the USB audio layer and immediately converted into
 * noncacheable 48-frame Float32 blocks for the later SAI consumer.
 *
 * Memory placement:
 *   - The byte ring lives in AXI SRAM (D1, 0x24000000), 64-byte aligned.
 *   - It is CPU-only scratch storage. TinyUSB owns separate endpoint buffers;
 *     the SAI-facing Float32 queue is in its own noncacheable MPU window.
 */
#ifndef AUDIO_BUFFER_H_
#define AUDIO_BUFFER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Ring buffer geometry.
// Depth: 1536 bytes = 8 ms of stereo 48 kHz / 16-bit PCM.
// These constants specify capacity only, not required USB transfer sizes.
#define AUDIO_BUFFER_PACKET_SIZE   32
#define AUDIO_BUFFER_PACKETS       48
#define AUDIO_BUFFER_DEPTH         (AUDIO_BUFFER_PACKETS * AUDIO_BUFFER_PACKET_SIZE)

// Debug counters (read via UART / LED, never in the isochronous ISR path).
typedef struct {
  volatile uint32_t rx_packets;      // successful pushes (not USB packet count)
  volatile uint32_t rx_bytes;        // bytes accepted into the ring
  volatile uint32_t push_overruns;   // pushes dropped due to full ring
  volatile uint32_t pop_underruns;   // pops returning less than requested
  volatile uint32_t last_feedback;   // last feedback value (16.16)
} audio_buffer_stats_t;

void     audio_buffer_init(void);
// Push arbitrary byte counts into the ring. Drops the entire push on overrun.
void     audio_buffer_push(uint8_t const *data, uint32_t n_bytes);
// Pop n_bytes from the ring. Returns bytes actually popped (0 on underrun).
uint32_t audio_buffer_pop(uint8_t *data, uint32_t n_bytes);
// Number of bytes currently in the ring; call from the consumer context.
uint32_t audio_buffer_available(void);
audio_buffer_stats_t const *audio_buffer_stats(void);

#ifdef __cplusplus
}
#endif

#endif
