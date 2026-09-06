# RHYTHM – Ziel-Plattform & Chip-Auswahl

> Stand: 2026-08-29
> Status: Entwürfe, weiter zu verfeinern

## Entscheidungen (final)

| Thema | Entscheidung | Begründung |
|-------|-------------|------------|
| MCU (EQ/Mixing) | STM32H743 | Deterministisch, I2S, DMA, Timer-Encoder, S/PDIF via GPIO |
| Dev-Board (Start) | NUCLEO-F767ZI (vorhanden) | Cortex-M7 @ 216 MHz, 1MB RAM, 2MB Flash, I2S, ST-Link |
| Dev-Board (Später) | NUCLEO-H743ZI2 | Gleiche Familie (H7), 480 MHz, mehr Reserve, Pin-ähnlich |
| BT-Sink + Web-Radio | ESP32-S3 (DevKitC-1) | A2DP-Sink + AVRCP + WiFi nativ, I2S-Output |
| DAC | ES9028 | 32-bit / 384 kHz, I2S, ~10 €, Mittelweg |
| ADC | TBD (CS5345 / ES9023) | Analog-Quellen digitalisieren, für EQ + digitale Ausgänge |
| TOSLINK-Empfang | PLR135/T | 3.3V, ~3 €, S/PDIF-Decoder in Software (AN5073) |
| Firmware-Toolchain | PlatformIO + VS Code | Kombinierbar mit STM32Cube (HW-Interfaces) |
| Prototyp-Phase 1 | Breadboard / Protoboard | Schnell iterieren |
| Prototyp-Phase 2 | Erste Prototyp-PCB (KiCad) | Nach Breadboard, dann finale PCB |
| Budget | 150–200 € ideal, 500 € Obergrenze | |

## Offen / zu klären

| Thema | Status |
|-------|--------|
| Chip 3 (Streaming: AirPlay, Spotify Connect) | **Offen** – RPi Zero 2 W als Option, aber bewusst nicht final. Weitere Optionen während der Entwicklung prüfen. |
| ADC-Modell | TBD – CS5345 (24-bit, 192 kHz, I2S, ~8 €) oder ES9023 (32-bit, 384 kHz, I2S, ~12 €) |
| ESP32-S3 vs. ESP32 (Original) | ESP32-S3 für Sink + WiFi. ESP32 Original nur falls A2DP-Source (BT-Output) auf MCU-Ebene gewünscht – aktuell auf Linux-Host delegiert. |
| ESP32-S3 Firmware-Stack | TBD – ESP-IDF vs. Arduino-Framework |
| RPi Zero 2 W OS | TBD – Raspberry Pi OS Lite 64-bit, PulseAudio, shairport-sync, Raspotify |
| S/PDIF-Decoder | Software auf STM32H743 (AN5073) vs. externer HW-Decoder (AK4396) |
| Analog-Signalpfad | TBD – Op-Amps, Filter, Pegelanpassung vor ADC / nach DAC |
| Gehäuse | TBD |

## Architektur-Überblick

```
                    ┌──────────────────────────────────────────────┐
                    │            RHYTHM – 3-Chip-Architektur         │
                    └──────────────────────────────────────────────┘

 ┌─────────────────────────────────────────────────────────────────────┐
 │  CHIP 1: STM32H743 (NUCLEO-F767ZI → NUCLEO-H743ZI)                  │
 │                                                                     │
 │  Aufgaben:                                                          │
 │  • 7-Band-Parametrik-EQ (7 Biquad, float, deterministisch)           │
 │  • Mixing / Mischpult (5 Quellen → 5 Ziele)                         │
 │  • Lautstärke + Mute                                                │
 │  • Encoder-Polling (8x: 7 EQ + 1 Volume)                            │
 │  • Taster-Polling (Quellwahl + Output-Buttons)                      │
 │  • LED-Steuerung (5 Output + Status)                                │
 │  • I2S-Master (48 kHz, 32-bit, stereo)                              │
 │  • S/PDIF-Empfang (TOSLINK → PLR135/T → GPIO → SW-Decoder)          │
 │  • S/PDIF-Übertragung (GPIO-Timing → TOSLINK-LED)                   │
 │  • I2C (ADC-Konfiguration)                                          │
 │  • SPI (Display, optional)                                          │
 └────────────┬────────────────────────────────────────────────────────┘
              │ I2S (48 kHz, 32-bit, stereo)
              │
 ┌────────────┴────────────────────────────────────────────────────────┐
 │  CHIP 2: ESP32-S3 (DevKitC-1)                                       │
 │                                                                     │
 │  Aufgaben:                                                          │
 │  • A2DP-Sink (BT-Empfang von Handy/Laptop)                          │
 │  • AVRCP (Play/Pause, Next/Prev, Volume)                            │
 │  • WiFi-Client (Internetradio / HTTP-Streams)                       │
 │  • I2S-Output → STM32H743 (Audio-Data)                             │
 │  • UART / I2C → STM32H743 (Kontroll-Signale: Quellstatus, AVRCP)   │
 └─────────────────────────────────────────────────────────────────────┘
              │ I2S (48 kHz, 32-bit, stereo)
              │
 ┌────────────┴────────────────────────────────────────────────────────┐
 │  CHIP 3: [OFFEN] – Streaming-Host                                   │
 │                                                                     │
 │  Option A: Raspberry Pi Zero 2 W (~15 €)                            │
 │  • AirPlay 2 (shairport-sync)                                       │
 │  • Spotify Connect (Raspotify)                                      │
 │  • UPnP/DLNA (optional)                                             │
 │  • BT-A2DP-Source (RHYTHM → BT-Lautsprecher, BT 4.2)                │
 │  • I2S / USB-Audio-Output → STM32H743                              │
 │                                                                     │
 │  Option B: [zu prüfen] – Alternativen während der Entwicklung       │
 │  • ESP32-S3 mit AirPlay-Server (begrenzt)                           │
 │  • STM32MP1 (Linux auf MCU, teurer)                                 │
 │  • Dediziertes Streaming-Modul (z.B. RPi Pico W + Custom-Stack)     │
 │                                                                     │
 │  → Entscheidung bewusst aufgeschoben                                │
 └─────────────────────────────────────────────────────────────────────┘

 ┌─────────────────────────────────────────────────────────────────────┐
 │  PERIPHERIE                                                         │
 │                                                                     │
 │  ADC (TBD: CS5345 / ES9023)                                        │
 │  • Stereo 1 (analog) → I2S → STM32H743                             │
 │  • Stereo 2 (analog) → I2S → STM32H743                             │
 │  • AUX (analog)    → I2S → STM32H743                               │
 │                                                                     │
 │  DAC (ES9028, 2x)                                                  │
 │  • I2S Input ← STM32H743                                           │
 │  • Analog-Output → Stereo / AUX / Kopfhörer (via Op-Amp)            │
 │                                                                     │
 │  TOSLINK-Empfänger (PLR135/T)                                      │
 │  • Optisch → TTL → GPIO → STM32H743 (SW-S/PDIF-Decoder)            │
 │                                                                     │
 │  TOSLINK-Sender (LED + Treiber)                                    │
 │  • GPIO ← STM32H743 → LED → Optisch                                │
 │                                                                     │
 │  Encoder (8x EC11 oder ähnlich)                                    │
 │  • Timer-Input-Capture auf STM32H743                                │
 │                                                                     │
 │  Taster (Quellwahl 6x, Output 5x, Status)                          │
 │  • GPIO auf STM32H743                                               │
 │                                                                     │
 │  LEDs (5x Output + Status)                                          │
 │  • GPIO / PWM auf STM32H743                                         │
 └─────────────────────────────────────────────────────────────────────┘
```

## Signalfluss (Audio)

```
QUELLEN                              EQ / MIX                            ZIELE
─────────                            ────────                            ─────
WiFi Radio  ──┐
BT (A2DP)   ──┤   ESP32-S3
SPDIF (TOS) ──┤   (I2S)       ┌─────────────────────────────┐
Stereo 1    ──┤   ADC         │  STM32H743                  │
Stereo 2    ──┼──────────────→│  • 7-Band-EQ (Biquad)       │──┬──→ DAC → Stereo
AUX         ──┘   (I2S)       │  • Mixing (5→5)             │  ├──→ DAC → AUX
                              │  • Volume / Mute            │  ├──→ DAC → Kopfhörer
                              └─────────────────────────────┘  ├──→ TOSLINK → SPDIF
                                                               └──→ [Chip 3] → BT-Speaker
```

## Budget-Schätzung (Prototyp Phase 1)

| Bauteil | Stück | Preis (ca.) |
|---------|-------|-------------|
| NUCLEO-F767ZI (vorhanden) | 1 | 0 € |
| ESP32-S3-DevKitC-1 (16MB Flash, 8MB PSRAM) | 1 | ~15 € |
| RPi Zero 2 W (falls Chip 3 = Option A) | 1 | ~15 € |
| ES9028 DAC (I2S, 32-bit/384 kHz) | 2 | ~20 € |
| ADC (CS5345 oder ES9023) | 2 | ~20 € |
| PLR135/T (TOSLINK-Empfänger) | 1 | ~3 € |
| TOSLINK-LED + Treiber | 1 | ~5 € |
| Encoder (8x EC11) | 8 | ~20 € |
| Taster + LEDs (11 + 5) | 1 | ~10 € |
| Op-Amps (analog Signalpfad) | 4 | ~10 € |
| Protoboard / Breadboard | 1 | ~15 € |
| Kabel, Jumper, Netzteil (5V 3A) | 1 | ~20 € |
| **Summe (ohne RPi)** | | **~120 €** |
| **Summe (mit RPi Zero 2 W)** | | **~135 €** |
| **Reserve (Unvorhergesehenes)** | | **~65 €** |
| **Gesamt (mit RPi, im Budget)** | | **~200 €** |

## Nächste Schritte

1. [ ] ADC-Modell finalisieren (CS5345 vs. ES9023)
2. [ ] Chip 3-Optionen weiter recherchieren (während der Entwicklung)
3. [ ] ESP32-S3 Firmware-Stack wählen (ESP-IDF vs. Arduino)
4. [ ] Analog-Signalpfad ausarbeiten (Op-Amps, Filter, Pegel)
5. [ ] Pin-Map für NUCLEO-F767ZI erstellen (I2S, I2C, SPI, GPIO, Timer)
6. [ ] Firmware-Gerüst aufsetzen (PlatformIO + VS Code)
7. [ ] Breadboard-Aufbau planen (Stufe für Stufe)
8. [ ] Erste BOM mit Bestelllinks erstellen
