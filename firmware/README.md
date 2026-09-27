# RHYTHM – UAC2 USB Audio Interface (STM32H753ZI)

## Projektziel
Hochwertiges USB Audio Interface:
- STM32H753ZI, USB 2.0 High Speed, externes ULPI PHY (USB3300)
- USB Audio Class 2.0
- 4 ADC-Eingänge, 2 DAC-Ausgänge, 24-bit in 32-bit Slots
- 48 / 96 / 192 kHz (später 44.1 / 88.2 / 176.4 / 384 kHz)
- Full Duplex, SAI/TDM, DMA, asynchrones USB-Audio-Feedback

## Phase 1 (aktuell) – Minimaler UAC2-Prototyp
- USB **Full Speed** (12 Mbps) über interne FS-PHY (PA11=DM, PA12=DP)
- **Kein externes ULPI PHY** auf dem Dev-Board → kein High Speed (480 Mbps)
- UAC2, 2-Kanal Playback, 48 kHz, 24-bit in 32-bit Slots
- Audiodaten werden in einen RAM-Ringbuffer geschrieben (kein SAI/DAC)
- Asynchrones Feedback (FIFO-count)
- **High Speed kommt später** mit einem USB3300 (ULPI)

## Build
```
cd firmware
pio run -e nucleo-h753zi
```
Ergebnis: `firmware/.pio/build/nucleo-h753zi/firmware.bin` (26.9 KB)

### Flashen
```
pio run -e nucleo-h753zi -t upload
```
Oder manuell mit OpenOCD / ST-Link:
```
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c "program firmware/.pio/build/nucleo-h753zi/firmware.bin verify reset exit"
```

## Architektur

```
PC / Host
│
│ USB 2.0 High Speed / UAC2
▼
USB3300 / ULPI PHY
│
▼
STM32H753ZI
│
├── TinyUSB UAC2 (Rhport 1 = OTG_HS, USB1_OTG_HS @ 0x40040000)
│   ├── EP0 (Control)
│   ├── EP1 OUT (ISO, 56 B, async, 48 kHz/2ch/4B)
│   └── EP81 IN (ISO feedback, 4 B, interval 4 = 16 ms)
│
├── USB RX FIFO (TinyUSB intern, 32 × 56 B = 1792 B, in AXI-SRAM)
│
├── audio_buffer (App-Ringbuffer, AXI-SRAM, 64-B-allo-Cache-Line)
│   ├── Push: usb_audio_task() (Main-Loop, 1 ms)
│   ├── Pull: (Phase 3) SAI/DMA
│   └── Overrun/Underrun-Counter (ISR-safe)
│
├── (Phase 3) SAI/TDM → DMA → DAC
└── (Phase 5) ADC → DMA → Ringbuffer → USB IN
```

### Dateistruktur
```
firmware/
├── platformio.ini              # Build-Konfiguration
├── tools/build_flags_h753.py   # Linker-ABI (FPv5-D16)
├── src/
│   ├── main.c                  # Main-Loop: tud_task + usb_audio_task + Debug
│   ├── main.h                  # HAL-Entry-Point
│   ├── board_clock.c           # 400 MHz (VOS1, HSI-PLL1) + HSI48 (USB)
│   ├── board_clock.h
│   ├── bsp.c                   # GPIO (LED, ULPI), UART, Delay
│   ├── bsp.h
│   ├── tusb_config.h           # TinyUSB-Konfiguration (OTG_HS Rhport1, UAC2)
│   ├── audio_buffer.c          # App-Ringbuffer (AXI-SRAM, 64-B-allo)
│   ├── audio_buffer.h
│   ├── audio_config.h          # Zentrale Audio-Konfiguration
│   ├── periodic.h              # Zeitbasierter Tick (LED)
│   └── usb/
│       ├── usb_audio.c         # UAC2-Callbacks + RX-Pump
│       ├── usb_audio.h
│       ├── usbd_desc.c         # UAC2-Descriptors (IAD, AC, CS-AC, AS)
│       └── usbd_desc.h
├── tools/
│   └── ST32CubeMX_project_for generation/   # CubeMX-Referenzprojekt (Pin/Interrupt-Generierung)
│       # ⚠ NICHT Teil des PlatformIO-Builds – nur als Referenz für die
│       #    Pin-/Interrupt-Konfiguration. Siehe build_src_filter in platformio.ini.
├── vendor/
│   ├── tusb/                   # TinyUSB (hathach/tinyusb, kopiert)
│   │   └── src/
│   │       ├── tusb.c          # Core
│   │       ├── tusb_fifo.c
│   │       ├── device/usbd.c
│   │       ├── class/audio/audio_device.c
│   │       └── portable/synopsys/dwc2/
│   │           ├── dcd_dwc2.c  # DWC2 Driver (STM32H7 ULPI)
│   │           └── dwc2_common.c
│   └── h753/                   # CMSIS + HAL (lokal)
│       ├── cmsis_core/
│       ├── cmsis_device/Include/stm32h753xx.h
│       ├── hal/
│       └── linker/STM32H753ZITX_FLASH.ld
└── docs/
    └── clock-verification.md   # Clock-Tree-Dokumentation
```

## Clock-Tree
```
HSI (64 MHz, ±1 %) ──→ PLL1 (M=4, N=100, P=2) ──→ 400 MHz (SYSCLK)
                                              └── Q=4 ──→ 400 MHz (ungenutzt)

HSI48 (48 MHz, dedizierter Oszillator, ±0.25 %) ──→ USB OTG HS (48 MHz)
    USBSEL in RCC->D2CCIP2R = HSI48 (beide Bits gesetzt)

APB1: 200 MHz, APB2: 400 MHz, APB3: 400 MHz, APB4: 200 MHz
```
Dokumentation: `docs/clock-verification.md`

## USB / PHY-Pins
STM32H753ZI: USB1_OTG_HS (Rhport 1) in **Full-Speed-Modus** (interne FS-PHY).
Kein externes ULPI PHY auf dem Dev-Board.

| Signal   | Pin  | AF    | Richtung |
|----------|------|-------|----------|
| USB_DM   | PA11 | AF10  | bidirektional (FS-PHY intern) |
| USB_DP   | PA12 | AF10  | bidirektional (FS-PHY intern) |
| VBUS     | PA9  | –     | Input (VBUS-Erkennung) |
| ID       | PA10 | –     | Input (OTG-Rolle, hier Device) |

### Voraussetzungen für funktionierenden USB (wichtig!)
1. **OTG_HS-Peripherie-Clock** aktiviert: `__HAL_RCC_USB1_OTG_HS_CLK_ENABLE()`
   (AHB1ENR.USB1OTGHSEN). Ohne diese liefert das GSNPSID-Register 0 und
   `tusb_init()` schlägt mit `ASSERT FAILED` in `dwc2_common.c` fehl.
2. **IRQ-Handler** `OTG_HS_IRQHandler()` definiert, der `dcd_int_handler(1)`
   aufruft. Ohne diesen bleibt der USB-Stack stumm (keine Enumeration).
3. **USB-48-MHz-Quelle** (HSI48) konfiguriert in `board_clock.c`.

**High Speed (480 Mbps)** erfordert ein externes ULPI PHY (z. B. USB3300) mit
12+ Leitungen (D0–D7, STP, DIR, NXT, CLK). Das kommt in Phase 2+ mit
Hardware-Änderung.

## UAC2-Descriptor-Topologie
```
IAD (8 B)
├── AC Interface (itf 0)
│   ├── CS-AC Header (9 B): bcdADC 2.00, category=Desktop Speaker
│   ├── Clock Source 0x04 (8 B): internal, fixed 48 kHz, freq read-only
│   ├── Input Terminal 0x01 (17 B): USB streaming, 2ch
│   ├── Feature Unit 0x02 (18 B): 3ch (master+L+R), mute+volume RW
│   └── Output Terminal 0x03 (12 B): speaker → FU 0x02
└── AS Interface (itf 1, alt 0/1)
    ├── alt 0: 0-bandwidth
    └── alt 1: streaming
        ├── CS-AS (16 B): Type I, 2ch physical
        ├── Type I Format (6 B): subslot 4 B, 24-bit resolution
        ├── ISO OUT EP 0x01 (7 B): async, 56 B, interval 1
        ├── CS-AS ISO EP (8 B): non-max-packets-ok
        └── ISO Feedback EP 0x81 (7 B): 4 B, interval 4 (16 ms HS)
```
Gesamte Config-Descriptor-Länge: 154 B (9 Config + 145 Func1)

## Feedback
Asynchrones USB-Audio mit FIFO-count-Methode:
- `tud_audio_feedback_params_cb()`: method = FIFO_COUNT, sample_freq = 48000
- TinyUSB regelt den FIFO-Füllstand automatisch und leitet den Feedback-Wert
  vom EP81 aus
- Kein SOF-ISR nötig

## Buffer-Struktur
1. **TinyUSB RX FIFO** (intern): 32 × 56 B = 1792 B, in AXI-SRAM (D1).
   - DMA-Puffer für USB OTG HS (TinyUSB verwaltet die D-Cache-Coherenz)
2. **App-Ringbuffer** (`audio_buffer.c`): 4 KB, in AXI-SRAM (D1), 64-B-allo-Cache-Line
   - Push: `usb_audio_task()` (Main-Loop, 1 ms)
   - Overrun/Underrun-Counter (ISR-safe)
   - Phase 3: SAI/DMA liest aus diesem Buffer

## STM32H7 DMA / Cache
- **USB OTG HS DMA**: kann NICHT auf DTCM (0x20000000) zugreifen
- **USB-Puffer**: AXI-SRAM (0x24000000, D1) oder D2-RAM (0x30000000)
- **TinyUSB D-Cache**: aktiviert (32-B-Line), verwaltet Coherenz intern
  (`CFG_TUD_MEM_DCACHE_ENABLE=1`)
- **App-Ringbuffer**: AXI-SRAM (D1), 64-B-allo-Cache-Line
  - Phase 1: CPU-only (kein Coherency-Problem)
  - Phase 3 (SAI/DMA): D-Cache-Maintenance erforderlich

## Debug
- UART (USART2, 115200): Status alle 2 s
  ```
  [UAC2] configured | 48000 Hz | rx=1234 pkt / 39488 B | overrun=0 underrun=0 |  STREAMING
  ```
- LED (PE1): toggle alle 10 s
- Keine printf im Isochronous-ISR-Pfad

## Testanleitung

### Windows
1. STM32 per USB an den PC anschließen
2. Geräte-Manager: "USB-Audio-Gerät" / "STM32H753 UAC2 Audio" sollte erscheinen
3. Audio-Eingabe/Ausgabe: Stereo, 48 kHz, 24-bit
4. Audio abspielen und UART-Log beobachten (rx-Packet-Counter steigt)

### Linux
1. `lsusb` → "STM32H753 UAC2 Audio"
2. `aplay -l` → Gerät sollte gelistet sein
3. `aplay -D plughw:1,0 /usr/share/sounds/alsa/Front_Center.wav`
4. UART-Log: `sudo stty -F /dev/ttyACM0 115200 && cat /dev/ttyACM0`

## Erwartetes Ergebnis (Phase 1)
1. ✅ Projekt baut fehlerfrei
2. ✅ STM32 enumeriert als UAC2-Gerät (2ch, 48 kHz, 24-bit)
3. ✅ Audiodaten treffen kontinuierlich ein
4. ✅ Daten werden in Ringbuffer geschrieben
5. ✅ Overrun/Underrun-Counter über UART sichtbar
6. ✅ USB-Verbindung stabil bei längerem Streaming

## Nächste Schritte (Phase 2-6)
- Phase 2: Audio-Pipeline vorbereiten (Module: usb_audio, audio_buffer, audio_output, audio_input, audio_clock)
- Phase 3: SAI-Ausgabe (USB → Ringbuffer → SAI → DMA → DAC)
- Phase 4: Sampleraten 48/96/192 kHz
- Phase 5: 4-Kanal ADC
- Phase 6: Full Duplex (4 IN + 2 OUT)
