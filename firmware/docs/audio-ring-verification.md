# Audio-Ringbuffer: Korrektur und Hardware-Readback (2026-09-27)

## Fehler und Korrektur

Die Kapazität beträgt 1536 Bytes, keine Zweierpotenz. Die bisherige
Bitmaskierung mit `DEPTH - 1` berechnete falsche Indizes und Belegungen.
Zusätzlich konnten vollständig belegter und leerer Ring nicht zuverlässig
unterschieden werden. Die Beschränkung auf Vielfache von 32 Bytes verwarf
gültige USB-/FIFO-Blöcke (z.B. 176/180 Bytes bei 44,1 kHz Stereo/16-bit).

Die Indizes laufen jetzt modulo doppelter Kapazität; der Speicherzugriff
erfolgt modulo Kapazität. Die zusätzliche Runde unterscheidet voll/leer.
Release/acquire-Atomics veröffentlichen abgeschlossene Kopien zwischen
einem Producer und einem Consumer. Beliebige Byteanzahlen werden akzeptiert;
bei Platzmangel wird weiterhin der gesamte Push verworfen.

`rx_packets` zählt erfolgreiche Pushes, nicht USB-Pakete.
`rx_bytes` zählt ausschließlich im Ring gespeicherte Bytes.

## Regressionstest

`tests/audio_buffer/test_audio_buffer.c` prüft volle Kapazität, Drop-on-full
ohne Überschreiben, Byte-Reihenfolge, wiederholte Index-/Speicher-Wraps,
variable USB-Blockgrößen, Teil-Reads, übergroße Pushes und Underrun.

Mit einem nativen C11-Compiler (hier temporär Zig cc):

```sh
cc -std=c11 -Wall -Wextra -Werror -Isrc src/audio_buffer.c tests/audio_buffer/test_audio_buffer.c -o test_audio_buffer
./test_audio_buffer
```

Vor Korrektur: Assertion `audio_buffer_available() == sizeof(input)` schlägt fehl.
Nach Korrektur: PASS. Firmware: `pio run -e nucleo-h753zi` SUCCESS.
Bestehende PlatformIO-Warnung: unbekannte Option `project_dir`.

## Flash und Messung

```powershell
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c 'gdb port disabled; tcl port disabled; telnet port disabled; program .pio/build/nucleo-h753zi/firmware.elf verify reset exit'
```

Ergebnis: `Verified OK`, danach Reset. Adressen aus dem **neu gebauten ELF**
per `arm-none-eabi-nm -n` ermittelt (bei jedem Neubau erneut prüfen):

| Symbol | Adresse |
|---|---|
| s_head | 0x200000bc |
| s_stats (5 Wörter) | 0x200000c0 |
| s_tail | 0x200000d4 |
| s_streaming (1 Byte) | 0x2000023a |
| uwTick | 0x20000c54 |

Readback via OpenOCD `halt; puts "[read_memory ADDRESS 32 COUNT]"; resume`.
Keine RAM-Manipulation, kein Reset zwischen den beiden Messungen.
Nach Resume 5000 ms gewartet, erneut angehalten und gelesen.

| Messwert | A | B |
|---|---:|---:|
| head | 1536 | 1536 |
| tail | 0 | 0 |
| erfolgreiche Pushes | 8 | 8 |
| gespeicherte Bytes | 1536 | 1536 |
| push_overruns | 0x481f | 0x5bb4 |
| pop_underruns | 0 | 0 |
| streaming | 1 | 1 |
| HAL-Tick | 0x4b2e | 0x5ede |

Delta: 5013 verworfene Pushes in 5040 Firmware-ms. Empfang und Drain aus dem
TinyUSB-FIFO bis zur Anwendung sind damit nachgewiesen. Der volle Ring nimmt
korrekt keine weiteren Daten auf. Aktuell existiert kein SAI/DMA-Consumer;
deshalb sind Drops nach dem Füllen erwartet. Dies verifiziert keine hörbare
Ausgabe, keine PCM-Signalqualität und keine exakte USB-Paketrate.
