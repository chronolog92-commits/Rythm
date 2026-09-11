# RHYTHM – Anforderungen

> Zusammenfassung der I/O- und Bedienanforderungen.
> Detaillierte technische Ausarbeitung folgt in den Teilordnern `hardware/`, `firmware/`, `mechanical/`.

## Eingänge (Quellen)

| # | Quelle | Schnittstelle | Anmerkung |
|---|--------|---------------|-----------|
| 1 | WiFi Radio | Netzwerkstream | Internetradio / AirPlay / Spotify Connect? |
| 2 | Bluetooth | A2DP / AVRCP | Drahtlose Quelle |
| 3 | SPDIF | Digitales Eingangssignal | CD-Player / Receiver |
| 4 | Stereo 1 | Analog (Cinch / Klinke) | |
| 5 | Stereo 2 | Analog (Cinch / Klinke) | |
| 6 | AUX | Analog (Cinch / Klinke) | Zusätzlicher analoger Eingang |

> Hinweis: Im README sind 5 Quellen genannt, aber 6 aufgelistet – zu klären, ob AUX als 5. analoge Quelle neben Stereo 1/2 zählt oder ob die Zählung korrekt ist.

## Ausgänge (Ziele)

| # | Ziel | Schnittstelle | Anmerkung |
|---|------|---------------|-----------|
| 1 | Stereo | Analog (Cinch / Klinke) | Hauptausgang |
| 2 | AUX | Analog (Cinch / Klinke) | |
| 3 | Kopfhörer | Klinke (+ Großstecker-Adapter) | |
| 4 | SPDIF | Digitaler Ausgang | |
| 5 | Bluetooth | A2DP-Sink / Audio-Streaming | |

## Bedienelemente

### 7-Band-Parametrik-EQ
7 Drehknöpfe, Frequenzbereiche (Vorschlag):
- 60 Hz
- 150 Hz
- 400 Hz
- 1 kHz
- 2.5 kHz
- 6 kHz
- 12 kHz

### Volume
- 1 großer Drehknopf für Gesamtlautstärke

### Quellwahl
- Einrastende Schalter / Taster für die Quellen (WiFi Radio, BT, SPDIF, Stereo 1, Stereo 2, AUX)
- Haptisches Feedback (Einrasten)

### Output-Buttons
- Beleuchtete Tasten für jede Ausgabe
- LED-Anzeige, ob die jeweilige Ausgabe aktiv ist

## Offene Punkte

- [ ] Ziel-Plattform / Mikrocontroller wählen (Raspberry Pi 4/5, ESP32-S3, STM32 + Linux, FPGA)
- [x] Audio-Codecs / DAC/ADC-Chips auswählen → **ES9039Q2M** (DAC), **CS5345** (ADC Phase 1) / **ES9023** (ADC Ziel-PCB)
- [ ] Analog-/Digital-Signalpfad und Filter (EQ, Mute, Crossover) definieren
- [ ] Gehäuse-Design / Frontpanel-Layout
- [ ] Firmware-Architektur (Linux + PulseAudio/ALSA, bare-metal, RTOS)
- [ ] Bluetooth-Stack (Module, Codec-Unterstützung, AVRCP)
- [ ] Netzwerk-/WiFi-Radio-Streaming (UPnP, AirPlay, Internetradio, Spotify Connect?)
- [ ] Prototypenplan: Breadboard / Protoboard vs. eigene PCB
- [ ] Budget-Schätzung und Bestellliste für Bauteile
- [ ] Lizenz festlegen
