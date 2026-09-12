# RHYTHM – Controller-Kapazitätsprüfung für 7×5 Stereo

> Stand: 2026-09-12 · Dokument-/Datenblattprüfung und ausgeführte Rechenprüfung.
> **Kein Hardware-Stresstest, kein vollständiger Pinmux-Nachweis, keine neue Firmware geflasht.**

## 1. Ergebnis

**STM32H753ZI als Dev-MCU und STM32H743 als Ziel-MCU bleiben grundsätzlich geeignete Kandidaten für 7 Stereo-Eingänge und 5 unabhängige Stereo-Ausgänge bei 48 kHz.** Die Kanalzahl zwingt nicht automatisch zu einem größeren Prozessor. Die alte Topologie und der vorgefundene Firmwarestand reichen jedoch **nicht** als Nachweis, dass das Gerät diese Funktionen bereits bedienen kann.

Die Architektur muss folgende Punkte berücksichtigen:

1. SAI/TDM und mehrere separate PCM-Datenpfade statt pauschal drei Stereo-I²S-Busse.
2. ES9842PRO für vier und ES9822PRO für zwei analoge Kanäle, kein analoger MUX.
3. Drei unabhängige DAC-Pfade für Stereo, AUX und Kopfhörer.
4. Classic-BT-Sink/Source-Hardware zusätzlich bzw. als geeigneter Host; **ESP32-S3 kann kein A2DP**.
5. Taktdomänenanpassung für TOSLINK, Funk und Netzwerk; USB-Audio-Device mit Feedback/Puffern.
6. Korrektur/Validierung der vorgefundenen CPU-/Busclock-Konfiguration.
7. Konfliktfreie Pins auf dem tatsächlichen Package und Board, DMA-/RAM-/Cache-Plan und reale Worst-Case-Messung.

**Freigabestatus: bedingt machbar, nicht implementiert/verifiziert.** 400 MHz ist die Rechenannahme für das Dev-Board, keine in diesem Arbeitsschritt gemessene Frequenz.

## 2. Prüfumfang und Kanalinventar

| Richtung | Quelle/Ziel | Monokanäle | Hardware-/Transportpfad |
|----------|-------------|------------|------------------------|
| Eingang | Stereo 1 | 2 | ES9842PRO CH1/2 |
| Eingang | Stereo 2 | 2 | ES9842PRO CH3/4, derselbe ADC |
| Eingang | AUX | 2 | ES9822PRO CH1/2 |
| Eingang | TOSLINK | 2 | SPDIFRX |
| Eingang | BT | 2 | Eigenständiger A2DP-Sink-/PCM-Pfad |
| Eingang | WiFi | 2 | ESP32-S3-/Netzwerk-PCM-Pfad |
| Eingang | USB | 2 | PC → USB-Audio-Device |
| **Summe RX** | **7 Stereo** | **14** | Sechs analoge + acht digitale Kanäle |
| Ausgang | Stereo | 2 | ES9039Q2M #1 |
| Ausgang | AUX | 2 | ES9039Q2M #2 |
| Ausgang | Kopfhörer | 2 | Eigener DAC #3 + Kopfhörerverstärker |
| Ausgang | TOSLINK | 2 | SAI S/PDIF-TX |
| Ausgang | BT | 2 | Eigenständiger A2DP-Source-/PCM-Pfad |
| **Summe TX** | **5 Stereo** | **10** | Sechs analoge + vier digitale Kanäle |

Geprüft wird vorsorglich die gleichzeitige Erfassung aller Quellen und unabhängige Ausgabe aller Ziele, nicht nur „eine Quelle auf alle Ausgänge spiegeln“. USB ist nur Stereo-Playback in das Gerät; keine USB-Aufnahme/Hostfunktion hinzugefügt.

## 3. Verfügbare MCU-Funktionsblöcke

ST nennt für H753: vier SAI-Instanzen, SPI/I²S-Schnittstellen, SPDIFRX, zwei USB-OTG-Schnittstellen, DMA1/2, BDMA und MDMA. Der lokale ST-CMSIS-H753-Header bestätigt SAI1–4 jeweils mit A/B-Subblock, SPDIFRX, BDMA sowie USB1_OTG_HS/USB2_OTG_FS.

**Zählung ist nicht gleich Anschlussfähigkeit:** SAI1/SAI4 teilen sich teilweise alternative Pinpositionen; Boardbelegung, Package und Pinmux können die gleichzeitige Verwendung einschränken. SAI4 benutzt den D3-/BDMA-Pfad. Acht SAI-Subblöcke sind deshalb nicht automatisch acht frei verdrahtbare Streams auf einem beliebigen Board.

### Beispiel einer logischen Ressourcenbelegung (noch keine Pin-Freigabe)

Die folgende Variante vermeidet die Abhängigkeit von SAI4, benötigt aber drei SPI/I²S-Pfade zusätzlich:

| Ressource | Aufgabe | Daten/Format | Wichtige Bedingung |
|-----------|---------|--------------|-------------------|
| SAI1 A | ES9842PRO RX | TDM4, 4×32 Bit | Lokaler Master-RX, ADC Slave |
| SAI1 B | ES9822PRO RX | Stereo-I²S, 2×32 Bit | Eigenes Stereo-Frameformat; nicht blind an TDM4 synchronisieren |
| SAI2 A | Stereo-DAC TX | Stereo-I²S | Lokale Clock |
| SAI2 B | AUX-DAC TX | Stereo-I²S | Synchron zu A möglich; eigene Datenleitung |
| SAI3 A | Kopfhörer-DAC TX | Stereo-I²S | Lokale Clock |
| SAI3 B | TOSLINK TX | SAI S/PDIF-Modus | Unabhängige Protokollkonfiguration/Kernelclock prüfen |
| SPI/I²S #1 | WiFi RX | Stereo-PCM | ESP32-S3 Clock-/Slavefähigkeit prüfen |
| SPI/I²S #2 | BT RX | Stereo-PCM | Gewähltes Sink-Modul muss Transport unterstützen |
| SPI/I²S #3 | BT TX | Stereo-PCM | Gewähltes Source-Modul muss Transport unterstützen |
| SPDIFRX | TOSLINK RX | S/PDIF → PCM | Separater DMA-Empfang + SRC |
| USB OTG FS | USB Eingang | Device, isochroner OUT + Feedback | Eigener Anschluss und gültige 48-MHz-Clock |
| I²C bzw. SPI-Control | Wandlerregister | ADCs + drei DACs | Adressen/CS kollisionsfrei |
| UART/GPIO | Frontend-Status / UI | Nicht Audio | SWD, Tasten, Encoder, LEDs mitplanen |

Diese Tabelle ist ein **Ressourcenentwurf**, kein generierter oder verdrahteter Schaltplan. Freie Kernelclock-Teiler, unterstützte S/PDIF-Subblöcke, A/B-Synchronisierung, Alternative Functions und alle herausgeführten Pins anhand RM0433/konkretem Datenblatt prüfen. Nicht benötigte BCLK/FS-Ausgänge im S/PDIF-Modus sparen Pins; SAI-Synchronisierung und geteilte lokale Takte ebenfalls. Bei Engpässen bleiben validierte ADC-TDM-Kaskade, DAC-TDM-Selektion oder ein anderes Package mögliche Optionen.

### Zusätzlich ausgeführter Package-Pincheck

Ein konkreter **kollisionsfreier Kandidat mit 34 Signalpins** wurde gegen die offiziellen ST-Open-Pin-Data-Dateien für **STM32H753ZITx und STM32H743ZITx** geprüft: alle Signal/Pinkombinationen vorhanden, keine doppelte Pinbelegung, PA13/PA14 für SWD frei. Das belegt mehr als eine reine Peripheriezählung, ist aber noch kein Board-/AF-Register-/Clock-Freigabenachweis.

| Funktion | Kandidatenpins |
|----------|----------------|
| SAI1 A: ES9842PRO TDM4 RX | MCLK PE2, FS PE4, SCK PE5, SD PE6 |
| SAI1 B: ES9822PRO Stereo RX | MCLK PF7, FS PF9, SCK PF8, SD PF6 |
| SAI2 A: Stereo-DAC TX | MCLK PE0, FS PD12, SCK PD13, SD PD11 |
| SAI2 B: AUX-DAC TX, synchron zu A | SD PE11; Takte von A gemeinsam verteilen |
| SAI3 A: Kopfhörer-DAC TX | MCLK PD15, FS PD4, SCK PD0, SD PD1 |
| SAI3 B: S/PDIF TX | SD PD9; Protokoll-/Kernelclock-Konfiguration separat |
| I²S1: WiFi RX | CK PA5, SDI PA6, WS PG10 |
| I²S2: BT RX | CK PB13, SDI PB14, WS PB12 |
| I²S3: BT TX | CK PC10, SDO PC12, WS PA15 |
| Optionale externe Kernelclock I2S_CKIN | PC9 |
| SPDIFRX1_IN0 laut ST-Pin-XML | PD7; tatsächlichen Kanalindex im HAL/RM prüfen |
| USB OTG FS | DM PA11, DP PA12, VBUS PA9 |
| I²C1 Wandler-Control | SCL PB8, SDA PB9 |

**Board-Konflikte offen:** PD9 kann durch ST-Link-VCP/USART3-RX belegt sein; PA15 wird statt JTAG verwendet (SWD bleibt möglich). Ethernet, LEDs, Lötbrücken, Header-Erreichbarkeit, Stromversorgung, Clock-Verteilung und weitere UI-/UART-Pins müssen am tatsächlichen Board geprüft werden. Keine Lötbrücke, Pininitialisierung oder Debug-Konfiguration wurde geändert. Dieser ZI-Package-Kandidat legt die finale H743-Bestellnummer nicht stillschweigend fest.

Quellen, beide live abgerufen und maschinell geprüft:
- [ST Open Pin Data: STM32H753ZITx](https://raw.githubusercontent.com/STMicroelectronics/STM32_open_pin_data/master/mcu/STM32H753ZITx.xml)
- [ST Open Pin Data: STM32H743ZITx](https://raw.githubusercontent.com/STMicroelectronics/STM32_open_pin_data/master/mcu/STM32H743ZITx.xml)

### DMA und RAM

- Neun serielle TX/RX-Endpunkte in der Tabelle plus SPDIFRX benötigen bei jeweils einem Datenstream etwa zehn DMA-Datenpfade, gegebenenfalls zusätzliche S/PDIF-Statusübertragung.
- DMA1/2 bieten zusammen 16 Streams; DMAMUX-Zuordnung, Konkurrenz, FIFO, Prioritäten und Buszugriff dennoch konkret prüfen.
- USB-Übertragung und Kopierstrategie separat budgetieren; nicht ohne Prüfung einen weiteren universellen DMA-Kanal voraussetzen.
- SAI4 als Alternative benötigt BDMA-kompatible Speicherbereiche. DMA1/2 können nicht beliebig auf DTCM zugreifen.
- DMA-Ringpuffer im zugänglichen SRAM, Cache-Line-Alignment und Clean/Invalidate bzw. MPU-Noncacheable-Zonen definieren. DSP-Arbeitsdaten bei Bedarf in DTCM mit expliziten Kopien.

## 4. Ausgeführte Lastrechnung

Basis: 48.000 Frames/s, 32-Bit-Slots bzw. Float32 (4 Byte), L/R pro Stereoquelle. Werte wurden mit Python berechnet; es sind **keine gemessenen CPU-Zeiten**.

| Größe | Rechnung | Ergebnis |
|-------|----------|----------|
| PCM-Eingang | 14 × 4 × 48.000 | 2.688.000 Byte/s |
| PCM-Ausgang | 10 × 4 × 48.000 | 1.920.000 Byte/s |
| PCM insgesamt | 24 × 4 × 48.000 | 4.608.000 Byte/s |
| Stereo-BCLK | 48.000 × 2 × 32 | 3,072 MHz |
| TDM4-BCLK | 48.000 × 4 × 32 | 6,144 MHz |
| Optionale TDM8-BCLK | 48.000 × 8 × 32 | 12,288 MHz |
| Alle PCM-Ping-Pong-Puffer, 48 Frames | 24 × 4 × 48 × 2 | 9.216 Byte |
| CPU-Zyklen pro 1-ms-Block bei 400 MHz | 400.000.000 / 1.000 | 400.000 Zyklen |
| Stereo-Routingmatrix | 7 × 5 × 2 × 48.000 | 3.360.000 MAC/s |

Bus-/DMA-Verkehr ist höher als die PCM-Nutzlast: wiederholtes Lesen/Schreiben, Packing, SRC, Cache-Wartung, Statusdaten und Ringpuffer berücksichtigen. Die 9.216 Byte sind nur die reinen lokalen PCM-Doppelpuffer; USB-, Funk-, Netzwerk-, SRC- und Filterpuffer kommen hinzu.

### Biquad-EQ

Ein normalisierter Biquad in üblicher Direktform benötigt pro Sample fünf Multiplikationen und vier Additionen; konkrete CMSIS-DSP-Implementierung, FMA und Speicherzugriffe bestimmen die CPU-Zyklen.

| EQ-Szenario | Multiplikationen/s | Additionen/s |
|-------------|--------------------|--------------|
| 7 Bänder auf allen 14 Eingangskanälen | 23.520.000 | 18.816.000 |
| 7 Bänder auf allen 14 Eingangs- **und** 10 Ausgangskanälen | 40.320.000 | 32.256.000 |

Hinzu kommen Matrix-MACs, Gain/Mute, SRC, USB, Interrupts und UI. Das zeigt eine plausible Größenordnung für einen Cortex-M7 bei 400 MHz, **keine garantierte Auslastung von 12–15 %**. Die alte „130k MAC/s“-EQ-Schätzung war für diese Aufgabe deutlich zu niedrig.

SRC-Aufwand hängt stark von Kanalzahl, Filterlänge und Ratenverhältnis ab und ist ohne Implementierung nicht seriös prozentual zu beziffern. Für die Freigabe Worst-Case-Zyklen mit DWT_CYCCNT messen, nicht nur durchschnittliche CPU-Auslastung. Deadline bei 48-Frame-Blöcken: 1 ms mit bewusst eingeplanter Reserve.

## 5. USB-Bandbreite

Ein neuer Stereo-Eingang bei 48 kHz und 32-Bit-Containern benötigt nominell **384 Byte/ms**. Das liegt unter der maximalen Full-Speed-Isochron-Paketnutzlast von 1.023 Byte; USB High Speed oder externer ULPI-PHY ist allein wegen dieses Stereo-Eingangs nicht erforderlich.

- Maximalpaketgröße wegen asynchronem Feedback und Takttoleranz nicht nur auf den Mittelwert setzen.
- USB-Class-Version und Wortbreiten offen; interne 32-Bit-Slots sind unabhängig von UAC-/Hostformaten.
- Explizites Feedback für asynchrones Playback, Start/Stop, Suspend/Resume und Hostkompatibilität testen.
- Eigene MCU-USB-Leitungen, VBUS-Erkennung und passende Clock erforderlich; Debug-USB ist kein Ersatz.

## 6. Harte Architekturkorrekturen

### ESP32-S3 / Bluetooth

ESP32-S3 unterstützt Bluetooth LE, nicht Bluetooth Classic; klassisches A2DP und AVRCP können ihm nicht einfach per Firmware hinzugefügt werden. ESP32-S3 bleibt WiFi-Frontend. BT-Eingang und BT-Ausgang brauchen geeignete Module/Hostlösung und getrennte PCM-Pfade. Gleichzeitigen RX/TX-Betrieb nicht allein aus einem Bluetooth-Datenblattlogo ableiten.

### TOSLINK / Clock-Recovery

H753/H743 besitzen SPDIFRX. Die ältere GPIO-Softwaredecoder-Planung entfällt. SAI kann für S/PDIF-Ausgabe genutzt werden. SPDIFRX ist **kein ASRC**; auch bei nominell 48 kHz driftet eine unabhängige Quelle. Lokales Audio-Clocking, USB-Feedback und adaptive SRC/Puffer müssen zusammenpassen.

### Unabhängiger Kopfhörermix

Ein Kopfhörerverstärker am Stereo-DAC liefert denselben Stereomix. Fünf unabhängig bedienbare Stereo-Ziele verlangen einen dritten analogen Stereo-DAC-Pfad; ES9039Q2M #3 ist deshalb eingeplant. Verstärker, Last und Versorgung bleiben offen.

## 7. Vorhandener Firmware-Clock-Blocker

Während der read-only Firmwareprüfung vorgefunden in `firmware/src/board_clock.c`:

```text
HSI = 64 MHz
PLLM = 4
PLLN = 100
PLLP = 2
VCO = 64 / 4 × 100 = 1.600 MHz
SYSCLK = 1.600 / 2 = 800 MHz, NICHT 400 MHz
```

Die Python-Rechnung bestätigt diesen Widerspruch. Zusätzlich setzt der Entwurf AHB=/1 und mehrere APB-Teiler=/1, was nicht ungeprüft als zulässiges 400-MHz-Busdesign verwendet werden darf. VCO-/CPU-/AHB-/APB-Grenzen, VOS und Flash-Latenz nach dem tatsächlichen Silizium prüfen.

Das ist ein **statischer Befund über den Quelltext**, keine Behauptung, das Board laufe gerade real mit 800 MHz; PLL-Lock kann fehlschlagen, es könnte andere Firmware geflasht sein. Kein Boardzugriff oder Flashen erfolgte. Der vorhandene, bereits geänderte Firmware-Arbeitsstand wurde nicht überschrieben. Warnhinweise wurden in Firmware-README und Clock-Dokumentation ergänzt; Korrektur als eigene Firmwareaufgabe.

USB-48-MHz- und präzise Audio-Referenzclock sind zusätzlich zur CPU-Clock auszulegen. HSE-Bestückung bzw. ST-Link-MCO/BYPASS am konkreten Board nachsehen, nicht aus dem alten Clock-Text als bewiesen übernehmen.

## 8. Erforderliche Abnahmetests

- [ ] CPU-, Bus-, Audio- und USB-Clocks mit Registerreadback und externer Zeit-/Clockmessung.
- [ ] Pinmux für H753ZI-Dev-Board und genaues H743-Zielpackage; Board-Lötbrücken, Ethernet/Debug/UI-Konflikte einschließen.
- [ ] Beide ADCs: sechs unterschiedliche analoge Testsignale gleichzeitig, Slot-/Kanalreihenfolge nachweisen.
- [ ] Drei DACs: unabhängige Muster auf Stereo, AUX, Kopfhörer; kein unfreiwilliges Spiegeln.
- [ ] TOSLINK RX bei unterschiedlichen unterstützten Sampleraten, Lockverlust und langfristigem Clock-Drift; TOSLINK TX getrennt davon.
- [ ] USB-Enumeration, Formate, Feedback, Paketschwankungen, Suspend/Resume und Kabelabzug.
- [ ] BT RX und BT TX gleichzeitig plus WiFi aktiv; getrennte Streams, Codecs, Rate-Matching und Funklatenz dokumentieren.
- [ ] Vollständige 7×5-Matrix mit maximal gewählter EQ-Last; DWT-Worst-Case-Zyklen, DMA-FIFO-/Underrun-/Overrun-Zähler und Cache-Kohärenz.
- [ ] Längerer Dauerlauf mit UI/Netzwerkaktivität; definierte Reserve unter 1-ms-Deadline, keine Pufferdrift oder Aussetzer.
- [ ] Analoge Messungen und Leistungsbudget; Datenblatt-THD+N ist kein Messwert der Platine.

## 9. Quellen und Nachweisgrenzen

Herstellerunterlagen bzw. unveränderte Hersteller-PDFs:

- [STM32H753ZI Produktseite](https://www.st.com/en/microcontrollers-microprocessors/stm32h753zi.html)
- [STM32H743ZI Produktseite](https://www.st.com/en/microcontrollers-microprocessors/stm32h743zi.html)
- [ST DS12117 Rev 7, H753-Datenblatt – abrufbarer Spiegel](https://www.dart.ru/cataloguenew/ics/stm32/stm32pdf/stm32h753.pdf): S. 2 Funktionsumfang; Pin-/AF-Tabellen u. a. S. 63, 65, 74, 93. Ältere Revision, konkrete aktuelle Teile-/Errata-Freigabe bleibt nötig.
- [RM0433, STM32H742/H743/H753/H750 Referenzmanual](https://www.st.com/resource/en/reference_manual/dm00314099-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [AN5073, Receiving S/PDIF audio stream](https://www.st.com/resource/en/application_note/an5073-receiving-spdif-audio-stream-with-the-stm32f4f7h7-series-stmicroelectronics.pdf)
- [ESP32-S3-Datenblatt v2.2, S. 1–3: Bluetooth LE, kein Classic-Controller](https://documentation.espressif.com/esp32-s3_datasheet_en.pdf)
- [ES9842PRO v0.5.1](https://www.esstech.com/wp-content/uploads/2025/10/ES9842PRO_DS_v0.5.1.pdf): vier ADC-Kanäle, I²S/TDM Master/Slave, S. 13–14 Formate/Slots.
- [ES9822PRO v0.5.2](https://www.esstech.com/wp-content/uploads/2025/04/ES9822PRO_DS_v0.5.2.pdf): zwei ADC-Kanäle, I²S/TDM Master/Slave, S. 11 I²C-Adressformat, S. 13–14 Formate/Slots.
- Lokaler ST-Hardwareheader: `firmware/vendor_h753/cmsis_device/Include/stm32h753xx.h`.

Direkte ST-PDF-Abrufe liefen in dieser Sitzung teilweise in Timeouts; H753-Funktionsumfang/Pinbeispiele wurden deshalb auch anhand des erreichbaren ST-Datenblattspiegels und des lokalen ST-Headers geprüft. Das ersetzt keinen finalen CubeMX-/Schaltplan-/Hardwaretest.

Lokales ausgeführtes Rechen-/Dokumentationsprüfskript: `.hermes/verification/verify_audio_architecture_20260912.py`. Es prüft Mathematik, lokale Markdown-Links und Diagrammstruktur/Bezeichnungen – nicht Laufzeitfähigkeit der Firmware.

### Tatsächlich ausgeführte Dokumentationschecks

- Rechen-/Konsistenzskript: **PASS**, acht Markdown-Dateien, lokale Links, keine überholten ADC-/MUX-Auswahlen.
- Excalidraw: **131 aktive Elemente**, eindeutige IDs, auflösbare Bindungen, Text/originalText konsistent.
- SVG: gültiges XML, neue ADCs, USB und beide MCU-Bezeichnungen vorhanden; keine alten ADC-/MUX-Beschriftungen.
- PNG-Vorschau des SVG visuell geprüft: sieben Eingänge, fünf Ausgänge, getrennte ADC-/DAC-Pfade und lesbare offene Punkte.
- `git diff --check`: bestanden; reine LF/CRLF-Hinweise sind keine Whitespace-Fehler.

Diese Checks betreffen Dokumente/Diagramme; sie ersetzen keinen der offenen Hardware- und Laufzeittests.
