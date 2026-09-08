# RHYTHM – Hardware-Signalpfad & Clocking

> Stand: 2026-09-07
> Status: Entwurf
> Referenz-Zustand: **Prototyp Phase 1/2** (NUCLEO-F767ZI + ESP32-S3-DevKitC + Dev-Boards).
> Die finale PCB wird diese Topologie 1:1 übernehmen; Abweichungen werden dort als „Ziel-PCB" markiert.
> Oberliegendes Dokument: [`docs/architecture.md`](../architecture.md) (System-Überblick).

## 0. Grundsätze

1. **Eine Clock-Domäne, ein Master.** Alle Audio-Teile laufen auf 48 kHz / 32-bit / stereo.
   Die I2S-Takte (MCLK, BCLK, LRCLK) stammen von **einem einzigen Master** (STM32).
2. **Audio-Bus ≠ Control-Bus.** I2S trägt nur Audio. Alles andere (Status, AVRCP,
   Quellwechsel) läuft über UART. Ein I2C-Bus für Codec-Konfiguration bleibt
   parallel erhalten (nur ADC-Konfiguration; DAC braucht keine Konfiguration).
3. **Kein Sample-Rate-Negotiation auf dem I2S-Bus.** Wer eine andere Rate braucht,
   sample-rate-konvertiert selbst (nur relevant, wenn Chip 3 eine andere Rate liefert;
   für Phase 1/2: alle Quellen laufen auf 48 kHz).
4. **Protektor-Pegel für S/PDIF** – der PLR135/T liefert ein 3.3V-TTL-Signal,
   das direkt an ein GPIO des STM32 angeschlossen wird (AN5073).

## 1. I2S-Bus-Topologie

### 1.1 Rollen

| Gerät | Rolle | Richtung |
|-------|-------|----------|
| STM32 (F767ZI) | **I2S-Master** (Clock-Generator) | Takte + Audio-Data |
| ESP32-S3 (DevKitC-1) | **I2S-Slave** (2. Peripherie: I2S0/1 als Slave) | Audio-Data → STM32 |
| ADC (CS5345 / ES9023) | **I2S-Slave** | Audio-Data → STM32 |
| ES9028 x2 (DAC) | **I2S-Slave** | Audio-Data ← STM32 |

> **Warum STM32 als Master?**
> - Der STM32 ist das Deterministik-Zentrum (EQ, Mixing, S/PDIF-Decoder).
> - Ein Master kann mehrere Slave-Busse betreiben (I2S2 für ADC, I2S3 für ESP32-S3,
>   I2S1 für die 2x ES9028).
> - ESP32-S3 als Slave: ESP-IDF unterstützt I2S-Slave-Modus nativ (i2s_std_mode,
>   `i2s_slot_mode` + `i2s_clk_config`).
> - ADC/DAC als Slave: CS5345, ES9023, ES9028 sind I2S-Slave-fähig (BCLK/LRCLK
>   extern, MCLK optional).

### 1.2 Busse (3x I2S)

```
STM32 (F767ZI)
├── I2S1 ──→ [ES9028 #1 (Stereo)] ──→ Op-Amp → Stereo-Ausgang
│       └──→ [ES9028 #2 (AUX)] ──→ Op-Amp → AUX-Ausgang
│           (Kopfhörer: vom selben ES9028 #1 über Op-Amp-Stufe,
│            oder eigener DAC – siehe Abschnitt 1.3)
│
├── I2S2 ──→ [ADC (CS5345/ES9023)] ← analoge Quellen (Stereo 1, 2, AUX)
│
└── I2S3 ←── [ESP32-S3 (I2S-Slave)] ← BT (A2DP) + WiFi-Radio
```

> **Warum 3x I2S?**
> - I2S1: **Ausgangs-Bus** (DACs). 2x ES9028 hängen parallel an I2S1 (beide Slave,
>   gleiche Takte).
> - I2S2: **Eingangs-Bus** (ADC). 1x ADC (Stereo). Die 3 analogen Quellen
>   (Stereo 1, Stereo 2, AUX) werden **vor dem ADC** analog gemischt/geschaltet
>   (Analog-Switch / Multiplexer), nicht digital.
> - I2S3: **BT/WiFi-Bus** (ESP32-S3). Getrennt von I2S1/I2S2, weil der ESP32-S3
>   einen eigenen I2S-Slave-Port braucht und kein Mux auf I2S-Bus-Ebene sinnvoll ist.
>
> **Alternative (weniger I2S-Peripherie):** I2S1 und I2S2 zu einem Bus zusammenfassen,
> ADC als **zweiter Slave** an I2S1 (Full-Duplex: ADC sendet, DAC empfängt, gleicher
> BCLK/LRCLK). Das spart eine I2S-Peripherie, aber:
> - CS5345/ES9023 als I2S-Slave im **Receive-Modus** (ADC → MCU) brauchen BCLK/LRCLK
>   von außen → OK.
> - ES9028 als **Transmit-Modus** (MCU → DAC) brauchen BCLK/LRCLK von außen → OK.
> - **Aber:** ADC und DAC auf demselben Bus = gemeinsame Takte, aber **getrennte
>   Data-Lines** (ADC-DOUT → STM32, STM32-DOUT → DAC-DIN). Das ist machbar,
>   aber der STM32 muss I2S **Full-Duplex** betreiben (1x I2S-Peripherie,
>   TX + RX parallel). F767ZI I2S unterstützt Full-Duplex.
> - **Entscheidung:** Für Phase 1/2 **3x I2S** (einfacher zu debuggen,
>   getrennte Buses). Für die finale PCB: **2x I2S** (Full-Duplex I2S1 für
>   ADC+DAC, I2S2 für ESP32-S3) – reduziert Routing-Komplexität.

### 1.3 Kopfhörer-Ausgang

- **Phase 1/2:** Kopfhörer-Ausgang geht vom **ES9028 #1** (Stereo-DAC) über
  eine **zusätzliche Op-Amp-Stufe** (z.B. OPA1612, niederohmig, für 32 Ω)
  auf die Klinke. Kein separater DAC.
- **Ziel-PCB:** Optionaler **dritter ES9028** (oder dedizierter Kopfhörer-DAC,
  z.B. ES9012) für sauberere Trennung (Kopfhörer = eigene Gain-Stufe,
  kein Cross-Talk mit Stereo/AUX).

### 1.4 I2C-Bus (Codec-Konfiguration)

| Bus | Master | Slave | Zweck |
|-----|--------|-------|-------|
| I2C1 (F767ZI) | STM32 | ADC (CS5345/ES9023) | Gain, Mute, Sample-Rate, Power-Down |
| I2C1 (F767ZI) | STM32 | ES9028 (optional) | Mute, Soft-Reset (ES9028 hat I2C-Interface) |

> ESP32-S3 hat **kein I2C** zum STM32 (Kommunikation läuft über UART).
> I2C ist ein **einseitiger Konfigurations-Bus** (STM32 → Codec), kein Audio-Bus.

## 2. Clocking

### 2.1 Takt-Generierung

```
STM32 (F767ZI)
├── PLL → SYSCLK (216 MHz)
│   └── I2S-Peripherie-Predivider → 48 kHz Basis
│       ├── LRCLK = 48 kHz
│       ├── BCLK  = 48 kHz × 32 = 1.536 MHz (32-bit)
│       └── MCLK  = 48 kHz × 256 = 12.288 MHz (für ADC/DAC, falls benötigt)
│
└── I2S-Master-Output
    ├── I2S1 → ES9028 x2 (BCLK, LRCLK, MCLK)
    ├── I2S2 → ADC (BCLK, LRCLK, MCLK)
    └── I2S3 → ESP32-S3 (BCLK, LRCLK, MCLK)
```

> **MCLK:** ES9028 und CS5345/ES9023 arbeiten mit MCLK = 256 × fs (oder 384 × fs).
> Bei 48 kHz: MCLK = 12.288 MHz. Der STM32 kann MCLK über den I2S-Peripheral
> (I2S_MCLK_EN) ausgeben. F767ZI: I2S_MCLK ist auf **PA7** (I2S2_MCLK) bzw.
> **PB15** (I2S3_MCLK) verfügbar – muss in der Pin-Map abgeglichen werden.

### 2.2 Clock-Verantwortlichkeiten

| Takt | Generator | Empfänger |
|------|-----------|-----------|
| LRCLK (48 kHz) | STM32 (I2S-Master) | ADC, ESP32-S3, ES9028 x2 |
| BCLK (1.536 MHz) | STM32 (I2S-Master) | ADC, ESP32-S3, ES9028 x2 |
| MCLK (12.288 MHz) | STM32 (I2S-Master) | ADC, ES9028 x2 (ESP32-S3: optional, nicht nötig) |
| SYSCLK (216 MHz) | STM32 (HSE 8 MHz → PLL) | Intern (CPU, Peripherie) |

> **ESP32-S3 als I2S-Slave:** ESP-IDF `i2s_std_mode` unterstützt Slave-Modus
> (BCLK/LRCLK extern). MCLK wird vom ESP32-S3 **nicht benötigt** (nur für
> interne PLL, die in Slave-Modus nicht aktiv ist).

### 2.3 Sample-Rate

- **Alle Audio-Pfade: 48 kHz / 32-bit / stereo.**
- TOSLINK-Eingang: S/PDIF kann 44.1/48/88.2/96/176.4/192 kHz tragen.
  Der **SW-S/PDIF-Decoder** (AN5073) sample-rate-konvertiert auf 48 kHz
  (oder gibt die Rate an den STM32 weiter, der dann die I2S-Peripherie
  neu konfiguriert).
  - **Phase 1/2:** S/PDIF-Eingang wird auf **48 kHz** gezwungen
    (einfachster Fall). 44.1 kHz Quellen (CD-Player) werden sample-rate-konvertiert.
  - **Ziel-PCB:** Optional: Sample-Rate-Detektion + dynamische I2S-Umstellung
    (komplexer, später).
- BT (A2DP): SBC/AAC/MP3 werden vom ESP32-S3 auf **48 kHz** dekodiert
  (ESP-IDF A2DP-Sink: 48 kHz ist Standard für SBC/AAC).
- WiFi-Radio: HTTP-Streams werden vom ESP32-S3 auf **48 kHz** sample-rate-konvertiert
  (libmad / custom decoder).

## 3. ADC-Anbindung

### 3.1 Analog-Voreinstufe (vor dem ADC)

```
Stereo 1 (Cinch L/R) ──→ [Op-Amp Buffer] ──┐
Stereo 2 (Cinch L/R) ──→ [Op-Amp Buffer] ──┤
AUX (Cinch L/R) ──→ [Op-Amp Buffer] ──┤
                                          │
                                          ▼
                                 [Analog-Switch (MUX)]
                                 3:1 (L/R getrennt)
                                          │
                                          ▼
                                 [ADC (CS5345/ES9023)]
                                 I2S-Slave → STM32 (I2S2)
```

- **Op-Amp Buffer:** Niederimpedanz-Puffer (OPA1612 oder NE5532),
  Pegelanpassung auf ADC-Eingangspegel (CS5345: ±2.5 V ref, ES9023: ±3 V ref).
  **Entscheidungsbedarf:** Genauer Op-Amp + Pegel werden im Analog-Signalpfad
  (separater Abschnitt, folgt) finalisiert.
- **Analog-Switch (MUX):** 3:1 Stereo-Switch (z.B. DG5081, 2x 3:1, oder
  2x DG5081 für L/R). Gesteuert über GPIO des STM32 (2x GPIO für
  Channel-Auswahl: 00=Stereo 1, 01=Stereo 2, 10=AUX).
  - **Alternative:** 3x separate ADCs (je 1 pro Quelle) – teurer, aber
    kein Mux-Nachhall, alle Quellen gleichzeitig verfügbar.
    **Für Phase 1/2: 1x ADC + MUX** (Budget).
    **Für Ziel-PCB: 3x ADC** (alle Quellen gleichzeitig, kein Mux).

### 3.2 ADC-Digitalanbindung

| Pin (ADC) | → STM32 (F767ZI) | I2S2 |
|-----------|-------------------|------|
| SDO (Audio-Data Out) | I2S2_SD | RX |
| SCLK (BCLK) | I2S2_SCK | (Slave empfängt) |
| WS (LRCLK) | I2S2_WS | (Slave empfängt) |
| MCLK | I2S2_MCLK (falls verfügbar) | (Slave empfängt) |
| SDA (I2C) | I2C1_SDA | Konfiguration |
| SCL (I2C) | I2C1_SCL | Konfiguration |
| CS# (Chip-Select, falls SPI-Interface) | – (CS5345/ES9023 sind I2S+I2C, kein SPI) | – |

> **CS5345 vs. ES9023 (ADC-Entscheidung, offen):**
> - CS5345: 24-bit / 192 kHz, I2S + I2C, ~8 €, einfacher.
> - ES9023: 32-bit / 384 kHz, I2S + I2C, ~12 €, besserer SNR.
> - **Für Phase 1/2: CS5345** (Budget, reicht für 48 kHz/24-bit).
> - **Für Ziel-PCB: ES9023** (besserer SNR, 32-bit, mehr Reserve).

## 4. S/PDIF-Pfad (TOSLINK)

### 4.1 Eingang (RX)

```
TOSLINK-Eingang (optisch)
    │
    ▼
[PLR135/T] (TOSLINK-Empfänger, 3.3V TTL-Output)
    │
    ▼ (3.3V TTL, 32×fs)
[GPIO des STM32] (Interrupt-gesteuert)
    │
    ▼
[SW-S/PDIF-Decoder] (AN5073)
    │
    ▼ (48 kHz, 32-bit, stereo)
[Audio-Puffer → Mixing-Engine → I2S1 (DAC)]
```

- **PLR135/T:** 3.3V, ~3 €, S/PDIF-Output an GPIO.
- **SW-S/PDIF-Decoder:** GPIO-Interrupt (oder Timer-Input-Capture) →
  Software-Dekodierung (AN5073). Sample-Rate-Detektion + Sample-Rate-Konvertierung
  auf 48 kHz.
- **GPIO-Pin:** In der Pin-Map finalisieren (F767ZI: GPIOA-GPIOH,
  hoher Prioritäts-Interrupt).

### 4.2 Ausgang (TX)

```
[Audio-Puffer → S/PDIF-Encoder]
    │
    ▼ (S/PDIF-Stream, 32×fs)
[GPIO des STM32] (Timer-gesteuerter Output)
    │
    ▼
[TOSLINK-LED + LED-Treiber (z.B. BTQ11 + Widerstand)]
    │
    ▼
TOSLINK-Ausgang (optisch)
```

- **GPIO-Output:** Timer-gesteuert (PWM oder Timer-Compare), S/PDIF-Frame
  (32 Bit pro Sample, 48 kHz → 1.536 MHz Bit-Rate).
- **LED-Treiber:** TOSLINK-LED (IR, 850–940 nm) + Seriellenwiderstand
  (z.B. 39 Ω) + optional NPN-Transistor (BTQ11) für Pegelanhebung.
- **GPIO-Pin:** In der Pin-Map finalisieren.

### 4.3 S/PDIF-Entscheidung (Software vs. Hardware)

- **Software (AN5073):** Kostenneutral, ~5 % CPU, flexibel.
  **Entscheidung: Software** (für Phase 1/2 und Ziel-PCB).
- **Hardware (AK4396 oder externer S/PDIF-Decoder):** Teurer (~5–10 €),
  aber CPU-frei. **Nicht gewählt** (CPU-Budget ist komfortabel).

## 5. Stromversorgung

### 5.1 Spannungsnetze

| Netz | Quelle | Verbraucher | Anmerkung |
|------|--------|-------------|-----------|
| 5 V / 3 A | externes 5V-Netzteil (USB-C / DC-Jack) | Alle | Hauptspannung |
| 3.3 V | LDO / Buck vom 5 V | STM32, ESP32-S3, PLR135/T, Op-Amps, MUX, LEDs | Digital-Logik |
| 3.3 V (analog) | **Separater LDO** (oder 3.3 V mit analogem Decoupling) | ADC, ES9028, analoge Op-Amps | **Analog-Netz, getrennt vom Digital-3.3V** |
| 1.2 V (optional) | LDO vom 3.3 V | ESP32-S3 (core, falls benötigt) | ESP32-S3: 3.3 V reicht (kein 1.2 V nötig für DevKitC) |

> **Analog/Digital-Trennung:**
> - **Digital-3.3V:** STM32, ESP32-S3, PLR135/T, MUX, LEDs.
> - **Analog-3.3V:** ADC, ES9028, analoge Op-Amps.
> - **Getrennte Decoupling** (100 nF + 10 µF pro Netz), **getrennte GND-Pfade**
>   (analog GND und digital GND, verbunden an einem Punkt: Star Ground).
> - **Für Phase 1/2 (Breadboard/Protoboard):** Ein 5V-Netzteil → 3.3V-LDO
>   (z.B. AMS1117-3.3) → alle Verbraucher. **Analog/Digital-Trennung wird
>   auf der Breadboard vernachlässigt** (für den Prototyp akzeptabel,
>   für die finale PCB nicht).
> - **Für Ziel-PCB:** 2x 3.3V-LDO (digital + analog), Star Ground,
>   separater analoger GND-Pour.

### 5.2 Decoupling (pro Chip)

| Chip | Decoupling |
|------|-----------|
| STM32 (F767ZI) | 100 nF + 10 µF an VDD/VSS, 10 µF an VDDA/VSSA (analog) |
| ESP32-S3 (DevKitC-1) | 100 nF + 10 µF an 3V3 (DevKitC hat eigenes Decoupling, prüfen) |
| ADC (CS5345/ES9023) | 100 nF + 10 µF an AVDD/DVDD, 10 µF an AVSS/DVSS |
| ES9028 x2 | 100 nF + 10 µF an AVDD/DVDD, 10 µF an AVSS/DVSS |
| PLR135/T | 100 nF an VCC |
| Op-Amps (OPA1612 / NE5532) | 100 nF + 10 µF an V+ / V- |
| MUX (DG5081) | 100 nF an VCC |

### 5.3 Stromverbrauch (geschätzt)

| Verbraucher | Strom (ca.) |
|-------------|-------------|
| STM32 (F767ZI, 216 MHz) | ~80 mA (aktiv, I2S+DMA+Interrupts) |
| ESP32-S3 (DevKitC-1, WiFi+BT aktiv) | ~180 mA (WiFi-Scan), ~80 mA (BT-only) |
| ADC (CS5345) | ~5 mA |
| ES9028 x2 | ~2 × 3 mA = 6 mA |
| Op-Amps (4x) | ~4 × 2 mA = 8 mA |
| MUX (DG5081) | ~0.5 mA |
| PLR135/T | ~1 mA |
| LEDs (10x, 20 mA) | ~200 mA (alle an) |
| **Summe (typisch)** | **~500 mA – 700 mA @ 3.3V** |
| **Summe (peak, WiFi+BT+alle LEDs)** | **~1 A @ 3.3V** |

> 5V/3A-Netzteil ist mit großem Vorrat dimensioniert.

## 6. Prototyp-Phasen

### Phase 1: Breadboard (Stufe für Stufe)

**Ziel:** Audio-Pfad validieren (I2S, EQ, Mixing, DAC-Output).

| Stufe | Inhalt | Validierung |
|-------|--------|-------------|
| 1 | STM32 (F767ZI) + ES9028 x2 + 5V-Netzteil | I2S-Master → DAC → analoger Output (Testsignal: Sinus 1 kHz) |
| 2 | + ESP32-S3 (BT + WiFi) | A2DP-Sink → I2S-Slave → DAC (BT-Musik aus Handy) |
| 3 | + ADC (CS5345) + 1x analoge Quelle (Stereo 1) | Analog → ADC → I2S → STM32 → DAC (Loopback) |
| 4 | + S/PDIF-Eingang (PLR135/T + CD-Player) | TOSLINK → SW-Decoder → DAC |
| 5 | + Encoder (8x) + Taster (11x) + LEDs (10x) | UI-Interaktion (EQ, Volume, Quellwahl) |
| 6 | + 3x analoge Quellen + MUX | Alle Quellen verfügbar |
| 7 | + S/PDIF-Ausgang (TOSLINK-LED) | Alle Ausgänge verfügbar |
| 8 | + Chip 3 (RPi Zero 2 W, falls gewählt) | AirPlay / Spotify → I2S/USB → STM32 |

> **Breadboard-Abweichungen vom Ziel-Zustand:**
> - Analog/Digital-Trennung: **nicht umgesetzt** (ein 3.3V-Netz).
> - MUX: **DIP-Schalter** (manuell) statt DG5081 (für Phase 1).
> - Kopfhörer-Ausgang: **direkt vom ES9028 #1** (keine separate Op-Amp-Stufe).
> - ESP32-S3: **USB-Debug-Kabel** an Laptop (UART-Debug), I2S per Jumper.

### Phase 2: Erste Prototyp-PCB (KiCad)

**Ziel:** Saubere Signalpfade, alle Funktionen auf einer Platine.

| Merkmal | Umsetzung |
|---------|-----------|
| Layer | 2-Layer (Top + Bottom), GND-Pour auf Bottom |
| Analog/Digital | Getrennte 3.3V-LDOs (digital + analog), Star Ground |
| I2S-Topologie | 3x I2S (wie Phase 1), aber **getrennte Data-Lines** (ADC-DIN, DAC-DOUT, ESP32-DIN) |
| MUX | DG5081 (digital gesteuert, 2x GPIO) |
| Kopfhörer | Separate Op-Amp-Stufe (OPA1612) |
| Decoupling | Vollständig (100 nF + 10 µF pro Chip) |
| S/PDIF | PLR135/T + TOSLINK-LED + Treiber |
| ESP32-S3 | DevKitC-1 auf Platine (oder ESP32-S3-WROOM-Modul, kleiner) |
| STM32 | F767ZI (oder H743ZI, falls NUCLEO-Formfaktor nicht mehr nötig) |
| Testpunkte | An allen I2S-Signalen, ADC-Ausgang, DAC-Eingang, MUX-Auswahl |

### Ziel-PCB (finale Version)

**Ziel:** Produktionsreife, saubere Audio-Performance.

| Merkmal | Umsetzung |
|---------|-----------|
| Layer | 4-Layer (Top, GND1, GND2, Bottom), analoger GND-Pour |
| I2S-Topologie | **2x I2S** (Full-Duplex I2S1 für ADC+DAC, I2S2 für ESP32-S3) |
| ADC | **3x ES9023** (alle Quellen gleichzeitig, kein Mux) |
| DAC | 3x ES9028 (Stereo, AUX, Kopfhörer) |
| Analog/Digital | 2x 3.3V-LDOs (digital + analog), Star Ground, separater analoger GND-Pour |
| Gehäuse | Aluminium- oder Stahl-Gehäuse (TBD) |
| Gehäuse-Befestigung | PCB mit M3-Schrauben, Gehäusesockel mit GND-Kontakten |
| Testpunkte | Vollständig (alle Audio-Signale, I2C, UART) |
| Serienfertigung | 5–10 Stück (PCB-Order bei JLCPCB oder Ähnlichem) |

> **Abweichungen Phase 1/2 → Ziel-PCB:**
> - 3x I2S → 2x I2S (Full-Duplex)
> - 1x ADC + MUX → 3x ADC (kein Mux)
> - 2x DAC → 3x DAC (separater Kopfhörer-DAC)
> - 1x 3.3V-LDO → 2x 3.3V-LDOs (digital + analog)
> - F767ZI/H743ZI (NUCLEO) → F767ZI/H743ZI (TQFP144, direkt auf Platine)
> - ESP32-S3 DevKitC-1 → ESP32-S3-WROOM-Modul (kleiner, direkt auf Platine)
> - DIP-Schalter → DG5081 MUX (digital gesteuert)

## Offene Punkte (für diesen Abschnitt)

1. [ ] **I2S-Pin-Map für F767ZI** (I2S1/2/3: SD, SCK, WS, MCLK – welche Pins?)
2. [ ] **MCLK-Verfügbarkeit** auf F767ZI (I2S2_MCLK: PA7? I2S3_MCLK: PB15? – abgleichen)
3. [ ] **ADC-Modell finalisieren** (CS5345 für Phase 1, ES9023 für Ziel-PCB – bestätigt?)
4. [ ] **Analog-MUX-Modell** (DG5081 3:1? 2x für L/R? – Datasheet prüfen)
5. [ ] **Op-Amp-Auswahl** für analoge Voreinstufe (OPA1612? NE5532? – Pegel + Impedanz)
6. [ ] **Kopfhörer-DAC** (Phase 1: ES9028 #1, Ziel-PCB: eigener DAC – bestätigt?)
7. [ ] **ESP32-S3 I2S-Slave-Pins** (DevKitC-1: welche Pins für I2S-Slave? – Datasheet prüfen)
8. [ ] **S/PDIF GPIO-Pins** (RX + TX, Interrupt-Priorität – abgleichen mit F767ZI-Pinout)
9. [ ] **Stromversorgung: 1x oder 2x 3.3V-LDO für Phase 2?** (Budget vs. Qualität)
10. [ ] **Gehäuse-Integration** (PCB-Maße, Befestigung, GND-Kontakte – TBD)
