"""Halt/read/resume Phase-2c diagnostics using symbols from the flashed ELF.

Run from firmware/: python tools/read_audio_blocks.py --interval-ms 5000
Requires OpenOCD on PATH and the PlatformIO ARM toolchain. Does not reset,
flash or modify RAM. The ELF must match the flashed image. MPU region 1 must
still be selected (as left by MPU_Config); the reader never changes selection.
"""
import argparse
import json
from pathlib import Path
import re
import struct
import subprocess


FIELDS = (
    "magic version epoch sample_rate input_bytes pcm_frames blocks_completed "
    "blocks_published blocks_dropped blocks_released partial_bytes partial_frames "
    "nonzero_samples"
).split()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", default=".pio/build/nucleo-h753zi/firmware.elf")
    parser.add_argument("--interval-ms", type=int, default=5000)
    args = parser.parse_args()
    if args.interval_ms < 0:
        parser.error("interval must be nonnegative")
    nm = Path.home() / ".platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-nm.exe"
    symbols = {}
    text = subprocess.check_output([str(nm), "-n", args.elf], text=True)
    for line in text.splitlines():
        columns = line.split()
        if len(columns) == 3:
            symbols[columns[2]] = int(columns[0], 16)

    reads = {
        "REPORT": (symbols["g_audio_block_report"], 32, len(FIELDS) + 96),
        "SAMPLES": (symbols["g_audio_blocks"], 32, 96),
        "RING_STATS": (symbols["s_stats"], 32, 5),
        "STREAMING": (symbols["s_streaming"], 8, 1),
        "TICK": (symbols["uwTick"], 32, 1),
        "WRITE": (symbols["s_write"], 32, 1),
        "READ": (symbols["s_read"], 32, 1),
        "STAGING": (symbols["s_staging"], 32, 96),
        "FRAME": (symbols["s_frame"], 8, 4),
        "FRAMES": (symbols["s_frames"], 32, 1),
        "FRAME_BYTES": (symbols["s_frame_bytes"], 32, 1),
        "MPU": (0xE000ED94, 32, 4),
    }
    commands = ["gdb port disabled", "tcl port disabled", "telnet port disabled", "init"]
    for snapshot in ("A", "B"):
        commands.append("halt")
        for name, (address, width, count) in reads.items():
            commands.append(f'puts "{name}_{snapshot}=[read_memory 0x{address:x} {width} {count}]"')
        commands.append("resume")
        if snapshot == "A":
            commands.append(f"sleep {args.interval_ms}")
    commands.append("shutdown")
    result = subprocess.run(
        ["openocd", "-f", "interface/stlink.cfg", "-f", "target/stm32h7x.cfg",
         "-c", "; ".join(commands)], capture_output=True, text=True,
        timeout=60 + args.interval_ms / 1000,
    )
    output = result.stdout + result.stderr
    if result.returncode:
        raise RuntimeError(output)

    def words(name, snapshot):
        match = re.search(rf"{name}_{snapshot}=([^\r\n]+)", output)
        if not match:
            raise RuntimeError(f"Missing {name}_{snapshot}:\n{output}")
        values = [int(word, 0) for word in match[1].split()]
        if len(values) != reads[name][2]:
            raise RuntimeError(f"Truncated {name}_{snapshot}: {match[1]}")
        return values

    snapshots = {}
    for snapshot in ("A", "B"):
        raw = words("REPORT", snapshot)
        report = dict(zip(FIELDS, raw))
        if report["magic"] != 0x41424C4B or report["version"] != 1:
            raise RuntimeError("Report ABI mismatch: check flashed ELF")
        pcm = [v if v < 0x80000000 else v - 0x100000000 for v in raw[len(FIELDS):]]
        floats = [struct.unpack("<f", struct.pack("<I", v))[0]
                  for v in words("SAMPLES", snapshot)]
        comparable = report["blocks_published"] > 0 and report["blocks_released"] == 0
        ctrl, rnr, rbar, rasr = words("MPU", snapshot)
        # A halt can catch the producer mid-update: this live comparison is
        # diagnostic, not a transactional snapshot like the retained slot 0.
        last_pcm = struct.unpack("<hh", bytes(words("FRAME", snapshot)))
        last_frame = (words("FRAMES", snapshot)[0] - 1) % 48
        staging = words("STAGING", snapshot)
        last_float = [struct.unpack("<f", struct.pack("<I", v))[0]
                      for v in staging[2 * last_frame:2 * last_frame + 2]]
        report.update({
            "queued": (words("WRITE", snapshot)[0] - words("READ", snapshot)[0]) & 0xFFFFFFFF,
            "tick": words("TICK", snapshot)[0],
            "streaming": words("STREAMING", snapshot)[0],
            "ring_stats": words("RING_STATS", snapshot),
            "first_block_conversion_matches": all(f == p / 32768.0 for f, p in zip(floats, pcm)) if comparable else None,
            "first_block_nonzero_samples": sum(p != 0 for p in pcm),
            "first_pcm_preview": pcm[:8],
            "first_float_preview": floats[:8],
            "live_last_pcm": last_pcm,
            "live_last_float": last_float,
            "live_last_frame_matches": all(f == p / 32768.0 for f, p in zip(last_float, last_pcm))
                if words("FRAME_BYTES", snapshot)[0] == 0 and report["pcm_frames"] > 0 else None,
            "mpu": [hex(v) for v in (ctrl, rnr, rbar, rasr)],
            "mpu_noncacheable_window_ok": bool(ctrl & 1) and rnr == 1 and
                (rbar & 0xFFFFFFE0) == 0x24001000 and rasr == 0x130C0017,
        })
        snapshots[snapshot] = report
    a, b = snapshots["A"], snapshots["B"]
    if a["epoch"] == b["epoch"]:
        snapshots["delta"] = {key: (b[key] - a[key]) & 0xFFFFFFFF for key in
                              ("tick", "input_bytes", "pcm_frames", "blocks_completed",
                               "blocks_dropped", "nonzero_samples")}
    else:
        snapshots["delta"] = "Stream reset between snapshots; counters not comparable"
    print(json.dumps(snapshots, indent=2))
    if not all(s["mpu_noncacheable_window_ok"] for s in (a, b)):
        raise RuntimeError("MPU verification failed")
    if any(s["first_block_conversion_matches"] is False for s in (a, b)):
        raise RuntimeError("PCM/Float32 comparison failed")


if __name__ == "__main__":
    main()
