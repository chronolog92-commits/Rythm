/* Phase 2c: PCM16 LE stereo -> 48-frame Float32 blocks. */
#ifndef AUDIO_BLOCKS_H_
#define AUDIO_BLOCKS_H_

#include <stdint.h>

#define AUDIO_BLOCK_FRAMES 48U
#define AUDIO_BLOCK_CHANNELS 2U
#define AUDIO_BLOCK_QUEUE_SIZE 4U

typedef struct {
  float samples[AUDIO_BLOCK_FRAMES][AUDIO_BLOCK_CHANNELS];
} audio_block_t;

/* All fields are 32-bit words for SWD block reads. Halt to stop updates;
 * a halt inside processing may still catch partially updated diagnostics.
 * Counters are per epoch; first_pcm corresponds to queue slot 0's first block.
 * Once a consumer releases that slot its contents may of course be replaced. */
typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t epoch;
  uint32_t sample_rate;
  uint32_t input_bytes;
  uint32_t pcm_frames;
  uint32_t blocks_completed;
  uint32_t blocks_published;
  uint32_t blocks_dropped;
  uint32_t blocks_released;
  uint32_t partial_bytes;
  uint32_t partial_frames;
  uint32_t nonzero_samples;
  int32_t first_pcm[AUDIO_BLOCK_FRAMES * AUDIO_BLOCK_CHANNELS];
} audio_block_report_t;

extern volatile audio_block_report_t g_audio_block_report;

/* Main-loop producer. Reset also clears the byte ring and NOLOAD sample RAM.
 * Reset requires a quiescent consumer with no borrowed block; Phase 3 must
 * stop/release the consumer before resetting or changing sample rate. */
void audio_blocks_reset(uint32_t sample_rate);
void audio_blocks_process(void);

/* Single consumer: acquire borrows the oldest block (NULL if empty).
 * Repeated acquire returns the same block until release. No access after
 * release. ready() is a consumer-context snapshot. */
const audio_block_t *audio_blocks_acquire(void);
void audio_blocks_release(void);
uint32_t audio_blocks_ready(void);

#endif
