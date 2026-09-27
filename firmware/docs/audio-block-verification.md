# Phase 2c: Float32-Audiobloecke verifiziert (2026-09-27)

Board: NUCLEO-H753ZI, TinyUSB UAC1, 48 kHz / PCM16 Stereo.

## Implementierter Datenpfad

```text
USB ISO OUT -> TinyUSB RX-FIFO -> PCM-Byte-Ring
            -> PCM16-LE/Float32-Konverter
            -> 4 x 48-Frame-Stereoblock
```

- Ein Block enthaelt 48 Stereo-Frames als Float32: `48 * 2 * 4 = 384` Bytes.
- PCM16 wird mit `sample / 32768.0f` skaliert; Kanalfolge L/R bleibt erhalten.
- USB-Transfergrenzen duerfen Samples, Frames und Bloecke beliebig teilen.
- Vier fertige Bloecke werden als SPSC-Queue bereitgestellt.
- Bei voller Queue bleibt ihr Inhalt unveraendert; der neu fertiggestellte Block
  wird verworfen und `blocks_dropped` erhoeht.
- Stream-/Ratenwechsel verwerfen alte Bytes, Teilframes und fertige Bloecke.
- Die Blockdaten liegen als `NOLOAD` im 4-KiB-MPU-Fenster ab `0x24001000`.
  MPU-Region 1 konfiguriert normalen, nicht cachebaren, nicht bufferbaren,
  shareable RW/XN-Speicher.

## Host-Regressionstests

Produktionscode wurde mit einem nativen C11-Compiler getestet:

```powershell
zig cc -std=c11 -Wall -Wextra -Werror -Isrc src/audio_buffer.c src/audio_blocks.c tests/audio_blocks/test_audio_blocks.c -o test_audio_blocks.exe
test_audio_blocks.exe
```

Ergebnis:

```text
PASS: PCM scaling/LR, all split points, USB chunks, ownership, queue overflow/wrap, reset
```

Abgedeckt sind alle 191 Trennstellen eines PCM-Blocks, wechselnde
176/180/192/196-Byte-Chunks, Kanalreihenfolge, signed Skalierung, Queue-Umlauf,
gehaltene Consumer-Slots, Drop-on-full und Reset. Der bestehende Ringtest bleibt
ebenfalls gruen.

## Build und Flash

```powershell
pio run -e nucleo-h753zi
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c 'gdb port disabled; tcl port disabled; telnet port disabled; program .pio/build/nucleo-h753zi/firmware.elf verify reset exit'
```

Ergebnis: Build `SUCCESS`, OpenOCD `Verified OK`, anschliessend Target-Reset.
Die bestehende PlatformIO-Warnung zur unbekannten Option `project_dir` bleibt.

ELF-Sektionen:

| Sektion | Adresse | Groesse | Typ |
|---|---:|---:|---|
| `.ram_audio_buf` | `0x24000000` | 1536 B | NOLOAD |
| `.audio_blocks` | `0x24001000` | 4096 B | NOLOAD, MPU-Fenster |

## Hardware-Readback

Reproduzierbar mit:

```powershell
python tools/read_audio_blocks.py --interval-ms 5000
```

Das Skript liest Symboladressen aus genau dem gebauten ELF, haelt den Controller
zweimal kurz an und liest Report, PCM-Referenz, Float32-Bloecke, Ring-Counter und
MPU-Register. Es schreibt weder RAM noch Flash.

Messung waehrend laufender Windows-Wiedergabe:

| Messwert | Snapshot A | Snapshot B | Delta |
|---|---:|---:|---:|
| HAL-Tick | 58333 | 63360 | 5027 ms |
| streaming | 1 | 1 | 0 |
| empfangene/konvertierte Bytes | 11153072 | 12125896 | 972824 |
| konvertierte PCM-Frames | 2788268 | 3031474 | 243206 |
| fertige 48-Frame-Bloecke | 58088 | 63155 | 5067 |
| veroeffentlichte Queue-Bloecke | 4 | 4 | 0 |
| wegen voller Queue verworfen | 58084 | 63151 | 5067 |
| Byte-Ring-Overruns | 0 | 0 | 0 |
| fertige Queue-Slots | 4 | 4 | 0 |

Der erste Queue-Block enthielt Start-Stille und war bitgenau `0 -> 0.0f`.
Zusaetzlich wurde ein aktuelles nicht-null Samplepaar aus Arbeitsblock und
PCM-Referenz verglichen:

| Kanal | PCM16 | Float32 gelesen | Erwartet `PCM/32768` |
|---|---:|---:|---:|
| L | -504 | -0.015380859375 | -0.015380859375 |
| R | -554 | -0.01690673828125 | -0.01690673828125 |

MPU-Readback: `CTRL=0x5`, `RNR=1`, `RBAR=0x24001001`,
`RASR=0x130c0017`. Damit stimmen Adresse, Groesse (4 KiB), Normal-Memory-Typ,
Nicht-Cachebarkeit, RW und XN mit der Firmwarekonfiguration ueberein.

## Aussagegrenze

Damit sind Empfang, Byte-Ring, fragmentunabhaengige PCM-Konvertierung,
48-Frame-Blockbildung, Float32-Inhalt und Overrun-Verhalten nachgewiesen.
Ohne Phase-3-Consumer muss die Queue voll bleiben und neue Bloecke verwerfen.

Nicht nachgewiesen sind SAI/DMA-Ausgabe, hoerbarer DAC-Output, Feedback-/Drift-
Stabilitaet ueber mehr als eine Minute oder der 44,1-kHz-Modus auf Hardware.
