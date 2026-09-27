# Phase 2c: Float32 block handoff

Approved in chat: retain the PCM byte ring, convert PCM16 little-endian stereo
to `float samples[48][2]`, queue four complete blocks, drop the newest complete
block on overflow. No SAI consumer or simulated playback clock in this phase.

## Data and ownership

The main-loop producer drains the byte ring after each TinyUSB read. A staging
block collects 48 frames even when reads split a sample/frame/block. Signed
PCM is scaled by 1/32768.0; channels remain interleaved L/R. Both advertised
rates (44100 and 48000 Hz) are retained; 48 frames equal 1 ms only at 48000 Hz.
Completed blocks are copied into a four-slot SPSC queue. Acquire borrows the
oldest slot; release makes it reusable. Producer publication and consumer
release use C11 acquire/release atomics. A full queue preserves all old slots
and discards the new block; conversion continues so framing is maintained.

Stream stop/start, USB configuration/unmount, and changed sample rate discard
old ring, partial frame, staging and queued blocks. Reset is a quiescent
operation: Phase 3 must stop its consumer and release any borrowed block first.
Diagnostics are per stream epoch; an epoch counter survives resets.

## Memory

Five 384-byte sample blocks (four slots plus staging) occupy `.audio_blocks`,
a NOLOAD section at 0x24001000 in AXI SRAM. Reserve/check a 4 KiB MPU window;
MPU region 1 maps it as normal noncacheable, nonbufferable, shareable, RW, XN.
The byte ring remains below that window. Linker assertions prevent overlap
or window overflow. Explicit reset initializes NOLOAD storage. Control state
and a versioned volatile diagnostic report reside in DTCM.

## Verification

Host tests exercise production C: scaling extrema, L/R order, all fragment
boundaries, 176/180/192/196-byte chunks, queue full/drop/wrap/held ownership,
partial reset and rate changes. Existing ring regression must remain green.
Firmware build and flash verification must pass. Read RAM using fresh ELF
symbols: counters, queue bytes, first-block PCM reference and Float32 values,
and MPU RBAR/RASR. With no consumer, four blocks remain queued while the
overrun counter grows. This is not a feedback/drift or audible-output test.
Record results and accurately update the external AI milestone plan.
