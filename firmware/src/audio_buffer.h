/*
 * audio_buffer.h - Application-level audio ring buffer (playback path, Phase 1).
 *
 * The TinyUSB audio driver receives isochronous packets into its internal
 * software FIFO (cache-coherency handled by the DWC2 port). This ring buffer
 * is the application-level buffer: audio is read out of the TinyUSB FIFO into
 * this ring buffer by the USB audio layer, and will later be consumed by the
 * SAI/DMA output (Phase 3).
 *
 * Memory placement:
 *   - The ring buffer lives in AXI SRAM (D1, 0x24000000), which is accessible
 *     by the CPU. It is 64-byte (cache-line) aligned.
 *   - It is NOT placed in DTCM (0x20000000) because the STM32H7 USB OTG HS
 *     DMA cannot transfer to/from DTCM (tightly-coupled memory). The internal
 *     TinyUSB EP buffer is already handled by the DWC2 port's D-Cache
 *     maintenance. This ring buffer is only touched by the CPU in Phase 1, so
 *     cache coherence is a non-issue; from Phase 3 onward it is also only
 *     touched by CPU + SAI DMA, and the SAI DMA path will get the same
 *     cache-line treatment.
 */
#ifndef AUDIO_BUFFER_H_
#define AUDIO_BUFFER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Ring buffer geometry.
// Depth: 48 packets of 32 bytes each = 1536 bytes ~ 80 ms of stereo 48 kHz.
// Packet size = 32 bytes = 48000/1000 Hz * 2 ch * 4 bytes = 4 samples/frame * 2 ch * 4 B.
#define AUDIO_BUFFER_PACKET_SIZE   32
#define AUDIO_BUFFER_PACKETS       48
#define AUDIO_BUFFER_DEPTH         (AUDIO_BUFFER_PACKETS * AUDIO_BUFFER_PACKET_SIZE)

// Debug counters (read via UART / LED, never in the isochronous ISR path).
typedef struct {
  volatile uint32_t rx_packets;      // packets received from USB
  volatile uint32_t rx_bytes;        // bytes received from USB
  volatile uint32_t push_overruns;   // pushes dropped due to full ring
  volatile uint32_t pop_underruns;   // pops returning less than requested
  volatile uint32_t last_feedback;   // last feedback value (16.16)
} audio_buffer_stats_t;

void     audio_buffer_init(void);
// Push n_bytes (multiple of packet size) into the ring. Drops on overrun.
void     audio_buffer_push(uint8_t const *data, uint32_t n_bytes);
// Pop n_bytes from the ring. Returns bytes actually popped (0 on underrun).
uint32_t audio_buffer_pop(uint8_t *data, uint32_t n_bytes);
// Number of bytes currently in the ring.
uint32_t audio_buffer_available(void);
audio_buffer_stats_t const *audio_buffer_stats(void);

#ifdef __cplusplus
}
#endif

#endif
