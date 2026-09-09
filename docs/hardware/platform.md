# RHYTHM – Ziel-Plattform & Chip-Auswahl

> Stand: 2026-08-29
> Status: Entwürfe, weiter zu verfeinern

## Entscheidungen (final)

| Thema | Entscheidung | Begründung |
|-------|-------------|------------|
| MCU (EQ/Mixing) | STM32H743 (final) | Deterministisch, I2S, DMA, Timer-Encoder, S/PDIF via GPIO |
| Dev-Board (aktuell) | NUCLEO-H753ZI (STM32H753, vorhanden) | Cortex-M7 @ 400 MHz, 1MB RAM, 2MB Flash, I2S, ST-Link – Dev-Board bis zur finalen HW |
| BT-Sink + Web-Radio | ESP32-S3 (DevKitC-1) | A2DP-Sink + AVRCP + WiFi nativ, I2S-Output |
| DAC | ES9028Q2M (x2) | 32-bit / 768 kHz, Hyperstream II, LQFP48, I2S + I2C + SLIMbus, ~15 €, Stereodac mit 4-Kanal-D/A (2 Kanäle frei für Kopfhörer/Gain) |
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
 │  CHIP 1: STM32H743 (final) – Dev-Board NUCLEO-H753ZI                  │
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
 │  DAC (ES9028Q2M, 2x)                                                │
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

## DAC-Spezifikation (ES9028Q2M)

> Finaler DAC für alle analogen Ausgänge. **2 Stück** für Phase 1/2 (Stereo + AUX),
> optional **3. Stück** auf der Ziel-PCB für den Kopfhörer-Ausgang (eigene Gain-Stufe).

### Teilenummer & Datenblatt

| Feld | Wert |
|------|------|
| Bezeichnung | **ES9028Q2M** (ESS / Atheros, „Sabre 32") |
| Typ | 32-Bit Stereo Low-Power Audio DAC |
| Architektur | Hyperstream II, 4-Kanal-D/A (2 Kanäle → L/R, 2 Kanäle frei für Gain/Kopfhörer) |
| Auflösung | 32-bit (native), Dither/Noise-Reduction intern |
| Raten | bis **768 kHz** (I2S); typischer Betrieb 48 kHz |
| Schnittstellen | **I2S** (Master- oder Slave), **I2C** (Konfiguration), **SLIMbus** (optional) |
| Gehäuse | **LQFP-48** (7×7 mm, 0.5 mm Pitch) |
| Versorgung | 3.3 V (DVDD/AVDD), analoges Decoupling empfohlen |
| Strom (typ.) | ~3 mA pro Kanal-Paar (low-power Modus) |
| Preis (ca.) | ~15 €/Stück |
| Datenblatt | [ES9028Q2M Datasheet (ES, v0.41)](https://www.esstech.com/wp-content/uploads/2022/09/ES9028Q2M-Datasheet-v0.41.pdf) |

### Warum ES9028Q2M (statt „ES9028" generisch)

- **32-bit / 768 kHz** – deutlich mehr Reserve als 384 kHz; 48 kHz-Betrieb liegt
  mit großem Vorrat im Spektrum, MCLK = 256×fs (12.288 MHz) wird sauber erzeugt.
- **I2S-Slave-fähig** – passt direkt zur Topologie „STM32 = einziger I2S-Master".
- **I2C-Interface** – Mute, Soft-Reset, MCLK-Konfiguration, Power-Down per Register
  (wird in `signal-path.md` §1.4 als I2C1-Adresse verdrahtet).
- **4-Kanal-D/A im Chip** – ein Chip reicht für **L + R**, die beiden zusätzlichen
  Kanäle lassen sich für eine **getrennte Kopfhörer-Gain-Stufe** oder für
  **Differential/Single-End-Umschaltung** nutzen, ohne dritten Chip.
- **Low-Power** – passt ins 5 V/3 A-Budget (2–3 Chips ≈ 6–9 mA).

### Anbindung im RHYTHM

- **I2S1 (STM32, Master)** → beide ES9028Q2M als **Slave** (BCLK/LRCLK/MCLK von außen).
- **I2C1 (STM32)** → Konfiguration (Mute, Soft-Reset) – beide DACs am selben Bus
  mit unterschiedlicher I2C-Adresse (7-bit, per Pin-Strapping/Default-Address).
- **Analog-Output** → Op-Amp-Puffer (OPA1612) → Stereo-Cinch / AUX-Cinch / Kopfhörer-Klinke.
- **MCLK:** 12.288 MHz @ 48 kHz (256×fs). Vom STM32-I2S-Peripheral (`I2S_MCLK_EN`)
  auf den MCLK-Pin (Pin-Map, s. `signal-path.md` §2).
- **Decoupling:** 100 nF + 10 µF an AVDD/DVDD, 10 µF an AVSS/DVSS (analoges Netz,
  getrennt vom Digital-3.3V, s. `signal-path.md` §5).

### Zu klären (offen für DAC)

1. [ ] I2C-Adresse der 2x ES9028Q2M finalisieren (Default-Address im Datasheet prüfen).
2. [ ] MCLK-Pin auf H743ZI abgleichen (I2S_MCLK, s. `signal-path.md` §2 – für H743 neu prüfen).
3. [ ] Kopfhörer-Ausgang: eigene Gain-Stufe vom 3. Kanal-Paar des ES9028Q2M
    (statt separatem 3. Chip) – als Ziel-PCB-Option bestätigt?

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
| NUCLEO-H753ZI (aktuell, Dev) | 1 | 0 € |
| ESP32-S3-DevKitC-1 (16MB Flash, 8MB PSRAM) | 1 | ~15 € |
| RPi Zero 2 W (falls Chip 3 = Option A) | 1 | ~15 € |
| ES9028Q2M DAC (I2S, 32-bit/768 kHz, LQFP48) | 2 | ~30 € |
| ADC (CS5345 oder ES9023) | 2 | ~20 € |
| PLR135/T (TOSLINK-Empfänger) | 1 | ~3 € |
| TOSLINK-LED + Treiber | 1 | ~5 € |
| Encoder (8x EC11) | 8 | ~20 € |
| Taster + LEDs (11 + 5) | 1 | ~10 € |
| Op-Amps (analog Signalpfad) | 4 | ~10 € |
| Protoboard / Breadboard | 1 | ~15 € |
| Kabel, Jumper, Netzteil (5V 3A) | 1 | ~20 € |
| **Summe (ohne RPi)** | | **~130 €** |
| **Summe (mit RPi Zero 2 W)** | | **~145 €** |
| **Reserve (Unvorhergesehenes)** | | **~55 €** |
| **Gesamt (mit RPi, im Budget)** | | **~200 €** |

## Nächste Schritte

1. [ ] ADC-Modell finalisieren (CS5345 vs. ES9023)
2. [ ] Chip 3-Optionen weiter recherchieren (während der Entwicklung)
3. [ ] ESP32-S3 Firmware-Stack wählen (ESP-IDF vs. Arduino)
4. [ ] Analog-Signalpfad ausarbeiten (Op-Amps, Filter, Pegel)
5. [ ] Pin-Map für NUCLEO-H743ZI erstellen (I2S, I2C, SPI, GPIO, Timer)
6. [ ] Firmware-Gerüst aufsetzen (PlatformIO + VS Code)
7. [ ] Breadboard-Aufbau planen (Stufe für Stufe)
8. [ ] Erste BOM mit Bestelllinks erstellen
