# Phase 2c Float32 Blocks Implementation Plan

> **For agentic workers:** Use executing-plans to implement inline, task-by-task.

**Goal:** Verify a four-slot queue of 48-frame Float32 stereo blocks from USB PCM.
**Architecture:** Byte ring -> fragment-safe converter -> staging -> SPSC queue.
**Tech Stack:** C11, STM32H753, TinyUSB, PlatformIO, OpenOCD, native Zig cc tests.
**Spec:** `docs/superpowers/specs/2026-09-27-phase2c-blocks-design.md`

## Global Constraints
- Four slots, 48 frames, two channels, 384 bytes per Float32 block.
- PCM16 LE scaled by 1/32768; preserve 44100/48000 Hz support.
- Drop newest complete block on overflow; no synthetic consumer in firmware.
- `.audio_blocks` at 0x24001000, 4 KiB noncacheable MPU region 1, NOLOAD.
- No commits requested. Preserve existing uncommitted ring fix.

### Task 1: Converter and queue
Files: new `src/audio_blocks.h`, `src/audio_blocks.c`,
`tests/audio_blocks/test_audio_blocks.c`.
Interfaces: `audio_blocks_reset(uint32_t sample_rate)`,
`audio_blocks_process(void)` drains audio_buffer; `audio_blocks_acquire(void)`
returns `const audio_block_t *`; `audio_blocks_release(void)` returns void.
`g_audio_block_report` is the RAM diagnostic block.
- [ ] Write executable regression tests for conversion/fragmentation, queue
  overflow and retained slot ownership, and stream reset.
- [ ] Compile tests with native C11 compiler; observe missing implementation.
- [ ] Implement converter and release/acquire queue; run tests to PASS.
- [ ] Run existing byte-ring tests unchanged.

### Task 2: USB and memory integration
Files: `src/usb/usb_audio.c/.h`, `src/main.c`,
`vendor_h753/linker/STM32H753ZITX_FLASH.ld`.
- [ ] Add `audio_blocks_process()` after each byte-ring push; break if TinyUSB
  read returns zero. Guard PCM format with compile-time assertions.
- [ ] Add `usb_audio_reset()` to clear streaming and all buffered state;
  call on mount/unmount and stream close. Reset on new stream/rate and clear
  TinyUSB FIFO under a short USB IRQ exclusion for rate changes.
- [ ] Initialize block module before USB; configure region 1 before MPU enable.
- [ ] Add NOLOAD section and linker assertions. Build with
  `pio run -e nucleo-h753zi`; inspect ELF section and symbol addresses.

### Task 3: Board verification and documentation
Files: new `docs/audio-block-verification.md`; update
`C:/Users/Philipp/Projekte/_AI-Plan/RHYTHM_meilenstein-audio-test.md`.
- [ ] Flash using documented OpenOCD `program ... verify reset exit` command.
- [ ] Read diagnostic block and sample buffers twice during user playback;
  compare accepted bytes/frames, four queued blocks, growing drops.
- [ ] Verify MPU region 1 and actual Float32 conversion using PCM reference.
- [ ] Document exact results and reproducible commands; mark only proven
  Phase 2c requirements complete, retain feedback/drift and SAI as open.
- [ ] Review diff and run `git diff --check`.
