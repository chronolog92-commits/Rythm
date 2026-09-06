# RHYTHM – Custom Audio Interface

RHYTHM ist ein selbstgebautes Audio-Interface / Digitaler Vorverstärker (AVR) mit digitalen und analogen Ein-/Ausgängen, 7-Band-Parametrik-EQ über Drehknöpfe, großem Volume-Regler und beleuchteten Output-Tasten.

## Projektziele

- Zentrale Schnittstelle für mehrere Audioquellen (WiFi Radio, Bluetooth, SPDIF, 2x Stereo, AUX)
- Flexible Ausgabe an mehrere Endpunkte (Stereo, AUX, Kopfhörer, SPDIF, Bluetooth)
- Musikalisches, haptisches Bedienkonzept: einrastende Quellwahltaster, Drehknöpfe für EQ/Volume, beleuchtete Output-Buttons
- Modularer Aufbau, um später neue Quellen/Ausgaben ergänzen zu können

## Anforderungen

### Eingänge (5 Quellen)
1. WiFi Radio (z. B. internetradio / Netzwerkstream)
2. Bluetooth (A2DP / AVRCP)
3. SPDIF (digitales Eingangsformat, z. B. von CD-Player / Receiver)
4. Stereo 1 (analog, Cinch / Klinke)
5. Stereo 2 (analog, Cinch / Klinke)
6. AUX (zusätzlicher analoger Eingang, Cinch / Klinke)

### Ausgänge (5 Ziele)
1. Stereo (analog, Cinch / Klinke)
2. AUX (analog, Cinch / Klinke)
3. Kopfhörer (Klinke + Großstecker-Adapter)
4. SPDIF (digitaler Ausgang)
5. Bluetooth (A2DP-Sink / Audio-Streaming)

### Bedienelemente
- **7-Band-EQ**: 7 Drehknöpfe (z. B. 60 Hz, 150 Hz, 400 Hz, 1 kHz, 2.5 kHz, 6 kHz, 12 kHz)
- **Volume**: 1 großer Drehknopf für Gesamtlautstärke
- **Quellwahl**: einrastende Schalter/Taster für die 5 analogen/digitalen Quellen (WiFi Radio, BT, SPDIF, Stereo1, Stereo2, AUX)
- **Output-Buttons**: beleuchtete Tasten für jede Ausgabe; Leuchtdiode zeigt an, ob die Ausgabe aktiv ist

## Erster Projektstand

- Projektname festgelegt: **RHYTHM**
- Grundlegende I/O- und Bedienanforderungen dokumentiert
- Repo-Struktur initiiert

## Nächste Schritte (offene Punkte)

- [x] Ziel-Plattform / Mikrocontroller wählen → **STM32H743 + ESP32-S3 + [Chip 3 offen]** (Details in `docs/hardware/platform.md` und `docs/architecture.md`)
- [x] DAC wählen → **ES9028** (Details in `docs/hardware/platform.md`)
- [x] ADC wählen → **TBD** (CS5345 / ES9023, in `docs/architecture.md` offener Punkt)
- [ ] Analog-/Digital-Signalpfad und Filter (EQ, Mute, Crossover) definieren
- [ ] Gehäuse-Design / Frontpanel-Layout (Größe, Knopf- und Tasterpositionen)
- [x] Firmware-Architektur → **PlatformIO + VS Code, STM32Cube-HAL, FreeRTOS (optional)** (Details in `docs/architecture.md`)
- [x] Bluetooth-Stack → **ESP32-S3 A2DP-Sink + AVRCP** (Details in `docs/architecture.md`)
- [ ] Netzwerk-/WiFi-Radio-Streaming → **ESP32-S3 (HTTP/HLS) + [Chip 3: AirPlay/Spotify, offen]**
- [ ] Firmware-/Software-Repo aufsetzen → **PlatformIO + VS Code** (Gerüst in `firmware/`, Details in `docs/architecture.md`)
- [x] Prototypenplan → **Breadboard → Prototyp-PCB (KiCad) → finale PCB** (Details in `docs/hardware/platform.md`)
- [x] Budget-Schätzung → **~200 € (mit RPi), ~135 € (ohne RPi)** (Details in `docs/hardware/platform.md`)
- [ ] Lizenz festlegen
## Repo-Struktur

```
rhythm/
├── README.md
├── .gitignore
├── docs/
│   ├── requirements.md
│   ├── hardware/
│   ├── firmware/
│   └── mechanical/
├── firmware/
├── pcb/
└── mechanical/
```

## Lizenz

(Offen – TBD)
