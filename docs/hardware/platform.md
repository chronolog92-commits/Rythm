# RHYTHM – Ziel-Plattform & Chip-Auswahl

> Stand: 2026-09-12 · Gewählte Komponenten von noch offenen Implementierungsentscheidungen getrennt.
> Umfang: **7 Stereo-Eingänge / 5 unabhängige Stereo-Ausgänge**.

## Entscheidungen

| Thema | Entscheidung | Einordnung |
|-------|-------------|------------|
| MCU Audio-DSP | STM32H743 für Zielplatine | SAI/TDM, SPI/I²S, DMA, SPDIFRX, USB; Pinmux/Package noch prüfen |
| Aktuelles Dev-Board | NUCLEO-H753ZI / STM32H753ZI | Vorhanden, 400-MHz-CPU-Ziel; aktuelle Clock-Konfiguration noch nicht freigabefähig |
| ADC Stereo 1 + Stereo 2 | **1× ES9842PRO** | Vier gleichzeitig wandelnde Kanäle, zwei Stereoquellen |
| ADC AUX | **1× ES9822PRO** | Zwei gleichzeitig wandelnde Kanäle, eine Stereoquelle |
| Analoger Quell-MUX | **Entfällt vollständig** | Separate ADC-Kanäle statt analoger Quellenauswahl |
| DAC-Familie | ES9039Q2M | Planungsbasis **3×** für Stereo, AUX und unabhängigen Kopfhörermix |
| WiFi-Frontend | ESP32-S3 / DevKitC-1 | Netzwerkdekodierung; separater Stereo-Audiopfad zum STM32 |
| Bluetooth | Sink- und Source-Hardware **offen** | ESP32-S3 besitzt kein Bluetooth Classic und unterstützt kein A2DP |
| USB-Eingang | STM32 USB-Audio-Device, PC als Host | Stereo-Eingang, initial 48 kHz; kein USB-Host/USB-Stick-Player |
| TOSLINK RX | PLR135/T als bisheriger Empfängerkandidat → SPDIFRX | Spannung/Logik und Beschaffung vor Schaltplan prüfen; ASRC/Rate-Matching separat |
| TOSLINK TX | SAI im S/PDIF-Modus → optisches Sendemodul | Konkretes Modul offen; kein per-Flanke-GPIO-Bitbanging |
| Toolchain | PlatformIO + VS Code + STM32Cube-HAL | FreeRTOS optional; vorhandenes Boot-Gerüst ist keine vollständige Audio-Firmware |
| Budgetziel | 150–200 € ideal, 500 € Obergrenze | Neue vollständige Kalkulation offen |

## ADC-Spezifikation und Zuordnung

Herstellerdatenblätter, geprüft am 2026-09-12:

| Merkmal | ES9842PRO | ES9822PRO |
|---------|-----------|-----------|
| Anzahl im Gerät | 1 | 1 |
| Analoge Audiokanäle | 4 | 2 |
| Rythm-Zuordnung | CH1/2 Stereo 1, CH3/4 Stereo 2 | CH1/2 AUX |
| Dynamikumfang, Mehrkanalbetrieb | 122 dB | 125 dB |
| THD+N, Mehrkanalbetrieb | −116 dB | −117 dB |
| PCM-Schnittstellen | I²S/TDM, Master oder Slave | I²S/TDM, Master oder Slave |
| Steuerung | I²C oder SPI | I²C oder SPI |
| Gehäuse | 40-Pin-QFN, 5×5 mm | 40-Pin-QFN, 5×5 mm |
| Rythm-Betrieb | 48 kHz, 32-Bit-Slots, TDM4 RX | 48 kHz, 32-Bit-Slots, Stereo-I²S RX |

Die höheren Mono-Summenwerte der Datenblätter gelten **nicht** bei Nutzung aller Kanäle für getrennte Quellen. ESS listet beide Teile als Active; als neuere Versionen werden ES9843PRO bzw. ES9823PRO genannt. Die Nutzerentscheidung bleibt ausdrücklich **ES9842PRO + ES9822PRO**, kein stiller Wechsel auf Nachfolger.

- [ES9842PRO Datenblatt v0.5.1](https://www.esstech.com/wp-content/uploads/2025/10/ES9842PRO_DS_v0.5.1.pdf)
- [ES9822PRO Datenblatt v0.5.2](https://www.esstech.com/wp-content/uploads/2025/04/ES9822PRO_DS_v0.5.2.pdf)
- [ESS Produktstatus und Übersicht](https://www.esstech.com/products-overview/analog-to-digital-converters/)

### Analoge Beschaltung

Jede Stereoquelle bekommt ihre eigene zweikanalige Eingangsstufe: Schutz, definierte Eingangsimpedanz, Pegelanpassung, Filter und differentieller ADC-Treiber entsprechend Datenblatt. Keine unbestätigten Full-Scale-Pegel oder internen analogen PGAs voraussetzen. Digitale Gain-Regelung verhindert kein analoges Clipping.

**Versorgung nicht aus der alten 3,3-V-Tabelle übernehmen:** Die ADC-Pinlisten nennen 4,5-V-Analogversorgung und 3,3-V-I/O; Referenzen und interne Core-Regler sind gesondert zu behandeln. Exakte zulässige Bereiche, Sequencing, Referenzkondensatoren und Strombedarf aus den Recommended Operating Conditions / Referenzschaltungen übernehmen. Das bisherige pauschale „3,3 V für alle Wandler“ ist verworfen.

### Digitale Beschaltung

Basisentwurf: ES9842PRO über eine eigene TDM4-Datenleitung, ES9822PRO über eine eigene Stereo-I²S-Datenleitung. Beide arbeiten als Audio-Slaves in der lokalen MCU-Audio-Domäne. Gemeinsame Taktherkunft, aber unterschiedliche Bitclock-Frequenzen entsprechend der Slotzahl; Details in [signal-path.md](signal-path.md).

Beide unterstützen auch TDM-Daisy-Chain; eine gemeinsame TDM8-Leitung ist eine **optionale Optimierung**, nicht Voraussetzung oder bereits erprobte Verdrahtung. Datenleitungen niemals ungeprüft parallel zusammenschalten.

I²C-Adressen per ADDR-Pins kollisionsfrei vergeben. ESS zeigt in den ADC-Tabellen Adressbytes inklusive R/W-Bit; diese nicht ungeprüft als 7-Bit-Adresse in Treiber übernehmen. Normaler I²C-Zugriff kann eine vorhandene MCLK verlangen; Reset-/Clock-/Register-Reihenfolge nach Datenblatt festlegen. SPI bleibt Alternative bei Adresskonflikten.

## DAC-Spezifikation (ES9039Q2M)

ES9039Q2M bleibt die gewählte Stereo-DAC-Familie. Für fünf unabhängig routbare Stereoausgänge sind davon drei analog:

| DAC-Pfad | Ziel | Nachfolgende Analogstufe |
|----------|------|-------------------------|
| ES9039Q2M #1 | Stereo | Line-Filter / Puffer / Treiber |
| ES9039Q2M #2 | AUX | Line-Filter / Puffer / Treiber |
| ES9039Q2M #3 (eingeplant) | Kopfhörer | Lastgerecht dimensionierter Kopfhörerverstärker |

Drei getrennte Stereo-Datenströme sind die robuste Planungsbasis; gemeinsame BCLK/LRCLK bedeuten nicht gemeinsame Audiodaten. Bei paralleler identischer Datenleitung würden alle DACs denselben Mix wiedergeben. Eine TDM-Slot-Selektion wäre eine später zu prüfende Alternative.

- Steuerzugriff und Clock-Initialisierung vor Freigabe des ungemuteten Ausgangs erforderlich. Keine unvalidierten Register-Magiewerte aus der alten Doku übernehmen.
- Adressen/Chip-Selects aller fünf Wandler zusammen planen; I²C-/SPI-Konfiguration und Audio-Slave-Modus nach konkreter Datenblattrevision validieren.
- Kopfhörer nicht direkt an einen DAC-Ausgang anschließen; ein Line-Op-Amp ist nicht automatisch ein geeigneter Kopfhörertreiber.
- Versorgung, Referenzen, Filter, MCLK und Strombedarf nach Datenblatt und Analogentwurf; frühere pauschale 3-mA- und Rail-Angaben sind keine belastbare BOM-Basis.
- [Bisher verlinktes ES9039Q2M-Datenblatt](https://www.mouser.com/datasheet/2/1082/ES9039Q2M_Datasheet_v0_1_3-3168946.pdf)

## Funk und optionaler Streaming-Host

- ESP32-S3 bleibt WiFi-Kandidat, **nicht BT-A2DP-Kandidat**.
- BT-Eingang erfordert Classic-BT-A2DP-Sink, BT-Ausgang Classic-BT-A2DP-Source. Für gleichzeitigen Betrieb getrennte Funk-/Audio-Pfade reservieren; konkretes Modul/Host noch nicht gewählt.
- Ein ursprünglicher ESP32 ist ein möglicher Classic-BT-Kandidat, aber keine Freigabe für beliebige gleichzeitige Sink-/Source-Profile oder Codecs.
- Ein optionaler Linux-Streaming-Host (z. B. Raspberry Pi Zero 2 W) bleibt offen. Er kann Netzwerkdienste und gegebenenfalls BT-Funktionen übernehmen, muss unabhängige Audioströme und benötigte Schnittstellen nachweisen.
- USB vom PC ist ein eigener Eingang und darf nicht stillschweigend als derselbe USB-Port für einen Streaming-Host verplant werden.
- [Espressif ESP32-S3-Datenblatt v2.2, S. 1–3: Bluetooth LE](https://documentation.espressif.com/esp32-s3_datasheet_en.pdf)

## BOM / Budgetstatus

Die frühere Prototyp-Gesamtsumme ist wegen anderer ADCs, eines dritten DAC-Pfads, USB und getrennter BT-Funktionen **ungültig**. Keine erfundenen Händlerpreise oder aus der alten Stückliste übernommenen Summen.

| Baugruppe | Zielmenge | Preisstatus |
|-----------|-----------|-------------|
| NUCLEO-H753ZI | 1 vorhanden | Kein neuer Boardkauf geplant |
| STM32H743 auf Zielplatine | 1 | Bestellnummer/Package und Preis offen |
| ES9842PRO | 1 | Händlerangebot offen |
| ES9822PRO | 1 | Händlerangebot offen |
| ES9039Q2M | 3 eingeplant | Händlerangebot offen |
| Analoge Eingangsstufen | 6 Monokanäle | Schaltung und Preis offen |
| Line-Ausgangsstufen | 4 Monokanäle | Schaltung und Preis offen |
| Kopfhörerverstärker | 1 Stereo | Leistung/Last/Preis offen |
| ESP32-S3 WiFi-Frontend | 1 | Bestand bzw. Händlerangebot prüfen |
| Classic-BT Sink-/Source-Funktionen | 2 unabhängige Pfade | Modulanzahl/Hostlösung und Preis offen |
| TOSLINK RX + TX | je 1 | Empfänger/Senderangebot offen |
| USB-Anschluss, ESD, VBUS-Erkennung | 1 Satz | Full-Speed-Device, konkrete Schaltung offen |
| Audio-Taktquelle / Clock-Verteilung | 1 Satz | Quelle, Jitter und Fanout offen |
| Encoder | 8 | Angebot offen |
| Quell-/Ausgangstasten | 7 + 5 | Angebot offen |
| LEDs | 5 Output + Status | Anzahl/Strom/Preis offen |
| Versorgung, PCB, Steckverbinder, Gehäuse | 1 Satz | Leistungsbudget und Angebot offen |
| Optionaler Streaming-Host | 0 oder 1 | Nicht Bestandteil einer bestätigten Gesamtsumme |

**Gesamtkosten: noch nicht bestimmbar.** Beschaffbarkeit einzelner Chips und gut dokumentierter Module getrennt prüfen; Produktstatus Active ist kein Nachweis lokaler Lagerware.

## Prototypenplan

1. CPU-Clock und Zeitbasis des H753 validieren; Firmwareänderung separat freigeben.
2. Ein DAC-Modul mit Testsignal, anschließend ADC-Loopback mit einem der gewählten ADCs.
3. ES9842PRO mit vier Kanälen und ES9822PRO mit zwei Kanälen gleichzeitig; sechs verschiedene Testsignale.
4. USB-Device-Audio und TOSLINK-SPDIFRX inklusive Drift-/Rate-Matching-Prüfung.
5. WiFi und getrennte Classic-BT-Pfade hinzufügen; alle Quellen gleichzeitig.
6. Drei DAC-Pfade, SAI-TOSLINK-TX und BT-TX unabhängig ausgeben, dann EQ/Mixer-Volllasttest.
7. Erst nach Pinmux-/Clock-/Leistungsfreigabe PCB ausarbeiten; QFN-Wandler auf geeigneten Modulen/Platinen, nicht direkt im Steckbrett.

Die vorhandenen KiCad-Dateien werden durch diese Dokuänderung nicht automatisch kompatibel; Schaltplan-/Layoutabgleich bleibt ein eigener Arbeitsschritt.
