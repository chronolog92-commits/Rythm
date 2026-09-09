# RHYTHM – Systemarchitektur

> Stand: 2026-08-29
> Status: Entwurf, weiter zu verfeinern

## Überblick

RHYTHM ist ein 3-Chip-System:

| Chip | Rolle | Board |
|------|-------|-------|
| STM32H743 | Audio-DSP, Mixing, UI | NUCLEO-F767ZI → NUCLEO-H743ZI |
| ESP32-S3 | BT-Sink, Web-Radio | ESP32-S3-DevKitC-1 |
| [OFFEN] | Streaming (AirPlay, Spotify) | TBD |

## Audio-Signalpfad

```
                    ANALOG                          DIGITAL
                    ──────                          ───────
Stereo 1 (Cinch) ──→ [Op-Amp] ──→ [ADC] ──┐
Stereo 2 (Cinch) ──→ [Op-Amp] ──→ [ADC] ──┤
AUX      (Cinch) ──→ [Op-Amp] ──→ [ADC] ──┤
                                         │
TOSLINK-Eingang ──→ [PLR135/T] ──→ [SW-S/PDIF-Decoder] ──┤
                                         │
BT-Eingang ──→ [ESP32-S3 A2DP-Sink] ──→ [I2S] ──────────┤
                                         │
WiFi-Radio ──→ [ESP32-S3 WiFi] ──→ [I2S] ───────────────┤
                                         │
Streaming ──→ [Chip 3] ──→ [I2S/USB] ──────────────────┘
                                         │
                                         ▼
                                  ┌─────────────┐
                                  │  STM32H743  │
                                  │             │
                                  │ • 7-Band-   │
                                  │   Parametrik│
                                  │   EQ        │
                                  │ • Mixing    │
                                  │   (5→5)     │
                                  │ • Volume    │
                                  │ • Mute      │
                                  └──────┬──────┘
                                         │
                                         ▼ I2S (48 kHz, 32-bit, stereo)
                                  ┌─────────────┐
                                  │  ES9028Q2M x2  │
                                  │  (DAC)      │
                                  └──────┬──────┘
                                         │
                    ┌────────────────────┼────────────────────┐
                    │                    │                    │
                    ▼                    ▼                    ▼
              [Op-Amp]             [Op-Amp]             [Op-Amp]
              Stereo Out           AUX Out            Kopfhörer Out
              (Cinch)              (Cinch)            (Klinke)

              TOSLINK-Ausgang: GPIO (STM32H743) → [LED-Treiber] → [TOSLINK-LED]
              BT-Ausgang:    [Chip 3] → A2DP-Source → BT-Lautsprecher
```

## Chip-1: STM32H743 (Audio-DSP + UI)

### Aufgaben
- 7-Band-Parametrik-EQ (7 Biquad-Filter, float, 48 kHz stereo)
- Mixing / Mischpult (5 Quellen → 5 Ziele, Routing-Matrix)
- Lautstärke (digitale Gain-Stufe)
- Mute (pro Ausgang)
- Encoder-Polling (8x: 7 EQ + 1 Volume, Timer-Input-Capture)
- Taster-Polling (Quellwahl 6x, Output 5x)
- LED-Steuerung (5 Output + Status, GPIO/PWM)
- I2S-Master (48 kHz, 32-bit, stereo, DMA-Double-Buffer)
- S/PDIF-Empfang (TOSLINK → PLR135/T → GPIO → SW-Decoder, AN5073)
- S/PDIF-Übertragung (GPIO-Timing → TOSLINK-LED)
- I2C (ADC-Konfiguration)
- SPI (Display, optional)
- UART (Kommunikation mit ESP32-S3)

### Peripherie-Nutzung (NUCLEO-F767ZI)
| Peripherie | Nutzung |
|------------|---------|
| I2S2 (oder I2S3) | I2S-Master zu DAC/ADC |
| I2C1 | ADC-Konfiguration |
| SPI1 | Display (optional) |
| UART2 | ESP32-S3 Kommunikation |
| UART3 | Debug / Chip 3 |
| Timer2 | Encoder 1 (EQ 60 Hz) |
| Timer3 | Encoder 2 (EQ 150 Hz) |
| Timer4 | Encoder 3 (EQ 400 Hz) |
| Timer5 | Encoder 4 (EQ 1 kHz) |
| Timer6 | Encoder 5 (EQ 2.5 kHz) |
| Timer7 | Encoder 6 (EQ 6 kHz) |
| TIM8_CH1 | Encoder 7 (EQ 12 kHz) |
| TIM9 | Encoder 8 (Volume) |
| GPIO | Taster, LEDs, S/PDIF TX/RX |
| DMA1/2 | I2S-Audio-Transfer (Double-Buffer) |

> **Hinweis:** Pin-Map ist vorläufig und muss mit dem NUCLEO-F767ZI-Pinout abgeglichen werden.

### Firmware-Stack
- PlatformIO + VS Code
- STM32Cube-HAL (für I2S, DMA, Timer, I2C, SPI, UART)
- FreeRTOS (optional, für Encoder-Polling + UI-Task)
- Biquad-EQ-Engine (float, 7 Biquad, 48 kHz)
- Mixing-Engine (5×5 Routing-Matrix, digitale Gain)
- S/PDIF-Decoder (Software, AN5073)
- S/PDIF-Encoder (GPIO-Timing)

### CPU-Budget (48 kHz, stereo, float)
| Aufgabe | CPU-Aufwand (ca.) |
|---------|-------------------|
| 7 Biquad-EQ | ~2% (130k MAC/s von ~200 MFLOPS) |
| Mixing (5→5) | ~1% |
| Volume / Mute | <0.1% |
| S/PDIF-Decoder | ~5% (bit-banging, GPIO-Interrupt) |
| Encoder-Polling | ~2% |
| Taster / LEDs | ~1% |
| I2S-DMA (Übertragungssteuerung) | ~1% |
| **Gesamt** | **~12–15% von 216 MHz** |

> Sehr komfortabel. Der F767ZI (216 MHz) reicht mit großem Vorrat.

## Chip-2: ESP32-S3 (BT-Sink + Web-Radio)

### Aufgaben
- A2DP-Sink (BT-Empfang von Handy/Laptop, SBC/MP3/AAC)
- AVRCP (Play/Pause, Next/Prev, Track-Info, Volume)
- WiFi-Client (Internetradio / HTTP-Streams / HLS)
- I2S-Output → STM32H743 (48 kHz, 32-bit, stereo)
- UART → STM32H743 (Kontroll-Signale: Quellstatus, AVRCP-Events, Track-Info)

### Firmware-Stack
- TBD: ESP-IDF vs. Arduino-Framework
- ESP32-S3 I2S-Peripherie (2x I2S, Full-Duplex)
- ESP32-S3 Bluetooth-Stack (BR/EDR, A2DP-Sink, AVRCP)
- WiFi-Stack (STA-Modus, HTTP-Client, TLS für HTTPS-Streams)

### Schnittstelle zu STM32H743
| Schnittstelle | Richtung | Daten |
|---------------|----------|-------|
| I2S (BCLK, LRCLK, DOUT) | ESP32-S3 → STM32H743 | Audio-Data (48 kHz, 32-bit, stereo) |
| UART (TX/RX) | bidirektional | Kontroll-Signale, Status, AVRCP-Events |
| GPIO (INT) | ESP32-S3 → STM32H743 | IRQ (Quellwechsel, BT-Verbindung) |

## Chip-3: [OFFEN] – Streaming-Host

### Anforderungen
- AirPlay 2 (shairport-sync oder äquivalent)
- Spotify Connect (Raspotify oder äquivalent)
- UPnP/DLNA (optional)
- BT-A2DP-Source (RHYTHM → BT-Lautsprecher)
- I2S / USB-Audio-Output → STM32H743

### Option A: Raspberry Pi Zero 2 W (~15 €)
- Broadcom BCM2703B0 (Cortex-A53, 1 GHz)
- 512 MB RAM, 16 GB eMMC (oder microSD)
- WiFi 802.11 b/g/n, BT 4.2
- I2S-Header, USB-OTG, GPIO
- Vorinstallierbar: Raspberry Pi OS Lite 64-bit
- Software: PulseAudio + shairport-sync + Raspotify
- BT-A2DP-Source via PulseAudio / BlueZ
- **Vorteile:** Günstig, Linux nativ, AirPlay/Spotify/UPnP alle verfügbar, BT-Source
- **Nachteile:** Zusätzlicher Chip, Linux-Stack (nicht deterministisch, aber nur für Streaming)

### Option B: [zu prüfen]
- ESP32-S3 mit AirPlay-Server (begrenzt, kein AAC-Dekodierung)
- STM32MP1 (Linux auf MCU, teurer, komplexer)
- Dediziertes Streaming-Modul (z.B. RPi Pico W + Custom-Stack)
- USB-Audio-Interface (z.B. Creative Sound Blaster) als Blackbox

### Entscheidung
**Bewusst aufgeschoben.** Während der Entwicklung weitere Optionen prüfen.
Erste Priorität: ESP32-S3 (BT + Web-Radio) + STM32H743 (EQ + Mixing) zum Laufen bringen.
Chip 3 wird ergänzt, wenn der Audio-Pfad stabil ist.

## Peripherie

### ADC (analog → digital)
- TBD: CS5345 (24-bit, 192 kHz, I2S, ~8 €) oder ES9023 (32-bit, 384 kHz, I2S, ~12 €)
- 3x analoge Quellen (Stereo 1, Stereo 2, AUX)
- I2S-Output → STM32H743
- I2C-Konfiguration

### DAC (digital → analog)
- **ES9028Q2M** (32-bit, 768 kHz, I2S + I2C, Hyperstream II, LQFP-48, ~15 €) x2
- I2S-Input ← STM32H743 (I2S1, beide als Slave)
- I2C-Input ← STM32H743 (I2C1, Mute/Soft-Reset)
- 4-Kanal-D/A pro Chip: 2 Kanäle → L/R, 2 Kanäle frei (Kopfhörer-Gain / Differential)
- Analog-Output → Op-Amp → Stereo / AUX / Kopfhörer
- Details: [`docs/hardware/platform.md`](hardware/platform.md) § „DAC-Spezifikation"

### TOSLINK
- Eingang: PLR135/T (3.3V, ~3 €) → GPIO → SW-S/PDIF-Decoder
- Ausgang: GPIO → LED-Treiber → TOSLINK-LED

### Encoder
- 8x EC11 (oder ähnlich), A/B + Push
- Timer-Input-Capture auf STM32H743
- Auflösung: 24–32 Schritte/Umdrehung

### Taster
- Quellwahl: 6x einrastende Schalter (WiFi, BT, SPDIF, Stereo 1, Stereo 2, AUX)
- Output-Buttons: 5x Taster (Stereo, AUX, Kopfhörer, SPDIF, BT)
- GPIO auf STM32H743

### LEDs
- 5x Output-LEDs (je 1 pro Ausgang, aktiv/inaktiv)
- Status-LEDs (Power, BT-Verbindung, WiFi-Verbindung, Signal)
- GPIO / PWM auf STM32H743

## Offene Punkte

### System / Architektur
1. [ ] ADC-Modell finalisieren (CS5345 für Phase 1, ES9023 für Ziel-PCB – bestätigt?)
2. [ ] Chip 3 – Optionen weiter recherchieren
3. [ ] ESP32-S3 Firmware-Stack (ESP-IDF vs. Arduino)
4. [ ] Analog-Signalpfad (Op-Amps, Filter, Pegelanpassung)

### Hardware – Signalpfad & Clocking
→ Detailliert in [`docs/hardware/signal-path.md`](hardware/signal-path.md)
5. [ ] Pin-Map für NUCLEO-F767ZI (final): I2S1/2/3 (SD, SCK, WS, MCLK), I2C1, UART2, GPIO (S/PDIF RX/TX, MUX, Encoder, Taster, LEDs)
6. [ ] MCLK-Verfügbarkeit auf F767ZI abgleichen (I2S2_MCLK: PA7? I2S3_MCLK: PB15?)
7. [ ] ESP32-S3 I2S-Slave-Pins (DevKitC-1) abgleichen
8. [ ] S/PDIF GPIO-Pins (RX + TX) + Interrupt-Priorität abgleichen
9. [ ] Analog-MUX-Modell (DG5081 3:1, 2x für L/R) – Datasheet prüfen
10. [ ] Op-Amp-Auswahl für analoge Voreinstufe (OPA1612? NE5532?)
11. [ ] Kopfhörer-DAC (Phase 1: ES9028Q2M #1, Ziel-PCB: eigener DAC / 3. Kanal-Paar) – bestätigt?
12. [ ] Stromversorgung Phase 2: 1x oder 2x 3.3V-LDO (digital + analog)?

### Firmware / Implementierung
13. [ ] S/PDIF-Decoder-Implementierung (Software, AN5073)
14. [ ] Mixing-Engine (Routing-Matrix 5×5, digitale Gain)
15. [ ] EQ-Engine (7 Biquad, float, 48 kHz)
16. [ ] Encoder-Polling (Timer-Input-Capture, De-Bounce)
17. [ ] Firmware-Gerüst (PlatformIO + VS Code) – **DONE 2026-09-06** (Boot-Skelett, kompiliert)
18. [ ] Breadboard-Aufbau (Stufe für Stufe, siehe signal-path.md §6)
19. [ ] BOM mit Bestelllinks
