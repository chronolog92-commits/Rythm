# RHYTHM – Anforderungen

> Stand: 2026-09-12 · Verbindlicher I/O-Umfang: 7 Stereo-Eingänge, 5 Stereo-Ausgänge.
> Implementierungs- und Hardwarefreigabe stehen aus; siehe [Controller-Check](hardware/controller-capacity.md).

## Eingänge (7 Quellen / 14 Kanäle)

| # | Quelle | Schnittstelle / Wandlung | Anforderung |
|---|--------|-------------------------|-------------|
| 1 | Stereo 1 | Analog → ES9842PRO CH1/CH2 | Gleichzeitig mit Stereo 2 und AUX digitalisieren |
| 2 | Stereo 2 | Analog → ES9842PRO CH3/CH4 | Ein gemeinsamer 4-Kanal-ADC für beide Stereoquellen |
| 3 | AUX | Analog → ES9822PRO CH1/CH2 | Separater 2-Kanal-ADC |
| 4 | TOSLINK | Optischer S/PDIF-Empfänger → STM32-SPDIFRX | Fremde Clock-Domäne; auf lokale 48 kHz anpassen |
| 5 | Bluetooth | A2DP-Sink + AVRCP nach Modulauswahl | Classic-BT-Modul/Host; unabhängig von WiFi |
| 6 | WiFi | Netzwerkstream → ESP32-S3 bzw. optionaler Host | Dekodierung und Rate-Matching vor Übergabe an DSP |
| 7 | USB | PC → USB-Audio-Device im STM32 | Neuer Stereo-Playback-Eingang; initial 48 kHz |

**ADC-Entscheidung:** Genau 1× ES9842PRO und 1× ES9822PRO. Alle sechs analogen Audiokanäle stehen gleichzeitig bereit. Kein analoger Quell-Multiplexer und keine MUX-Auswahl-GPIOs mehr.

**USB-Scope:** PC ist Host, RHYTHM ist Device und empfängt Stereo-PCM. USB-Host-Betrieb, USB-Speicherwiedergabe und USB-Aufnahmeausgang sind nicht Teil dieser Anforderung. UAC-Version und unterstützte PCM-Wortbreiten sind vor Implementierung festzulegen; interne 32-Bit-Slots verlangen nicht automatisch 32-Bit-USB-PCM.

AirPlay, Spotify Connect und UPnP sind optionale Netzwerkdienste innerhalb der WiFi-Quelle, kein zusätzlicher achter Eingang.

## Ausgänge (5 Ziele / 10 Kanäle)

| # | Ziel | Schnittstelle / Wandlung | Anforderung |
|---|------|-------------------------|-------------|
| 1 | Stereo | Eigener ES9039Q2M → Line-Treiber | Unabhängiger Stereomix |
| 2 | AUX | Eigener ES9039Q2M → Line-Treiber | Unabhängiger Stereomix |
| 3 | Kopfhörer | Eigener DAC-Pfad → Kopfhörerverstärker | Kein bloßer Abgriff vom Stereo-Line-Mix |
| 4 | TOSLINK | SAI S/PDIF-TX → optischer Sender | Lokaler 48-kHz-Ausgang |
| 5 | Bluetooth | A2DP-Source → BT-Lautsprecher/Kopfhörer | Gleichzeitig mit BT-Eingang als Zielanforderung |

Planungsbasis für sechs unabhängig wandelbare analoge Ausgangskanäle: **3× ES9039Q2M**. Kopfhörerverstärker nach Lastimpedanz, Ausgangsleistung, Schutz und Versorgung auswählen; DAC nicht direkt an Kopfhörer anschließen.

## Signalverarbeitung

- 7×5-Stereo-Routingmatrix: L/R bleiben jeweils gepaart; alle Quellen gleichzeitig erfassen und alle Ziele unabhängig bedienen ist die Kapazitäts-Prüfbasis.
- 48 kHz lokale Audio-Domäne, Float32-Verarbeitung und 32-Bit-Transport-Slots.
- 7-Band-Parametrik-EQ. Bedienzuordnung (global, pro Quelle oder pro Ziel) noch festzulegen; Kapazitätsbericht rechnet konservativ auch mehrere EQ-Instanzen.
- Gain, Mute und klickfreie Übergänge; Summen-Headroom bzw. Clipping-Schutz bei gleichzeitigen Quellen.
- TOSLINK und Funk-/Netzwerkquellen benötigen Taktdomänenanpassung; gleicher nomineller Sample-Takt allein reicht nicht.
- USB-Feedback bzw. definierte Rate-Matching-Strategie verhindert langfristigen Pufferdrift.
- Gesamtlatenz und Filterlatenz erfassen; Bluetooth hat codec-/funkbedingte Zusatzlatenz und ist nicht automatisch synchron zu analogen Ausgängen.

## Bedienelemente

- Sieben EQ-Drehknöpfe, Frequenzvorschlag: 60 Hz, 150 Hz, 400 Hz, 1 kHz, 2,5 kHz, 6 kHz, 12 kHz.
- Ein großer Volume-Regler.
- Sieben Quellwahl-Schalter/Taster: Stereo 1, Stereo 2, AUX, TOSLINK, BT, WiFi, USB.
- Fünf beleuchtete Output-Tasten: Stereo, AUX, Kopfhörer, TOSLINK, BT.
- Tasten-Rastung, Mehrfachauswahl und Zuordnung zu Routing-/EQ-Ebenen im UI-Entwurf festlegen.
- Acht Encoder benötigen keine acht Hardware-Encoder-Timer: entprelltes GPIO-Scanning oder Erweiterungsbaustein als Planungsbasis; Pinbudget prüfen.

## Entscheidungen und offene Freigaben

- [x] Dev-MCU STM32H753ZI; Ziel-MCU STM32H743.
- [x] ADCs ES9842PRO + ES9822PRO, ohne analogen Quell-MUX.
- [x] DAC-Familie ES9039Q2M; drei unabhängige analoge Ausgangspfade eingeplant.
- [x] USB als siebter Stereo-Eingang.
- [x] Firmware-Toolchain PlatformIO + STM32Cube-HAL, RTOS optional.
- [ ] Korrekte CPU-/Bus-/Audio-/USB-Clocks nachweisen.
- [ ] Pinmux, Clock-Routing und DMA-Mapping für das tatsächliche MCU-Package bestätigen.
- [ ] Classic-BT-Sink und -Source auswählen: ESP32-S3 unterstützt keines von beiden.
- [ ] Analog-Frontend, Filter, Versorgung und Kopfhörerstufe auslegen.
- [ ] USB-Deskriptoren, Treiberkompatibilität und Feedback testen.
- [ ] Sieben Quellen / fünf Ziele unter Volllast mit Drift-, Latenz- und Dropout-Messung prüfen.
- [ ] Gehäuse, Frontpanel, BOM/Bestellliste und Projektlizenz festlegen.
