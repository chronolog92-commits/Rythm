# RHYTHM – Firmware

STM32H743 (Dev: NUCLEO-F767ZI, später NUCLEO-H743ZI2).
Toolchain: **PlatformIO** + repo-interner HAL (vendor/).

## Einmaliges Setup

```bash
# 1. PlatformIO installieren
python -m pip install platformio

# 2. HAL/CMSIS/Startup aus dem PlatformIO-Paket nach vendor/ kopieren
python tools/vendor_import.py

# 3. Plattform patchen (bar.py + platform.json + Board)
python tools/setup_platform.py
```

## Build & Flash

```bash
cd firmware
python -m platformio run            # Build
python -m platformio run -t upload  # Flash über ST-Link
python -m platformio device monitor # UART-Debug (115200)
```

VS Code: PlatformIO-Extension öffnen, `firmware/` als Workspace laden.

## Aufbau

```
firmware/
├── platformio.ini     Env: nucleo-f767zi, framework=bar
├── tools/
│   ├── vendor_import.py   HAL/CMSIS/Startup nach vendor/ (einmalig)
│   ├── setup_platform.py  bar.py + platform.json + Board patchen (einmalig)
│   └── bar.py             Framework-Skript (wird nach ~/.platformio kopiert)
├── vendor/
│   ├── cmsis_core/        CMSIS Core (core_cm7.h, cmsis_gcc.h, …)
│   ├── cmsis_device/      STM32F7xx Device (Include + Source/Templates)
│   ├── hal/               STM32F7xx HAL (Inc + Src, 12 Module)
│   ├── linker/            STM32F767ZI_FLASH.ld
│   └── startup/           startup_stm32f767xx.s (GCC)
└── src/
    ├── main.c         Phase 1: Boot-Blink (PC13) + UART-Debug
    ├── main.h         HAL-Konfiguration (manuell, kein CubeMX)
    ├── bsp.h          Pin-Map (vorläufig) + BSP-API
    └── bsp.c          RCC/GPIO/UART-Init, printf-Retarget
```

## Framework „bar"

Eigenes Framework-Skript (`tools/bar.py`), das:
- HAL-Module aus `vendor/hal/Src` als `libFrameworkHAL.a` baut
- `system_stm32f7xx.c` als `libFrameworkCMSIS.a` baut
- `startup_stm32f767xx.s` als `libFrameworkStartup.a` baut
- CPU-Flags (Cortex-M7, hard-float, nano.specs) setzt
- Linker-Skript aus `vendor/linker/` verwendet

Kein `framework = stm32cube` → kein Package-Download beim Build.

## Phasenplan

1. **Phase 1 (aktuell):** Boot-Blink + UART-Debug. Verifiziert: Clocks, GPIO, HAL, Toolchain.
2. **Phase 2:** I2S-Master 48 kHz/32-bit stereo + DMA-Double-Buffer → ES9028 (Sinus-Testtone).
3. **Phase 3:** 7-Band-EQ (Biquad, float) + Volume/Mute.
4. **Phase 4:** Mixing-Engine (5×5 Routing-Matrix).
5. **Phase 5:** Encoder-Polling (Timer-Input-Capture) + Taster/LEDs.
6. **Phase 6:** S/PDIF-RX (SW-Decoder, AN5073) + S/PDIF-TX.
7. **Phase 7:** UART-Protokoll zu ESP32-S3 (A2DP/WiFi-Quellen).

## Offene Punkte

- [ ] Pin-Map mit NUCLEO-F767ZI-Pinout final abgleichen (bsp.h)
- [ ] ESP32-S3-Protokoll definieren (UART, Frames)
- [ ] FreeRTOS-Entscheidung: rein HAL+DMA-ISR oder Tasks für UI/Encoder

## Hinweis

Pin-Belegungen in `bsp.h` sind **vorläufig** (aus docs/architecture.md).
Bevor die Breadboard-Phase startet, müssen I2S/UART/Pinouts gegen das
tatsächliche NUCLEO-F767ZI-Board verifiziert werden.
