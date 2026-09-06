# RHYTHM – Firmware

STM32H743 (Dev: NUCLEO-F767ZI, später NUCLEO-H743ZI2).
Toolchain: **PlatformIO + STM32Cube-HAL**.

## Aufbau

```
firmware/
├── platformio.ini     Env: nucleo-f767zi (stm32cube, 115200 Monitor)
└── src/
    ├── main.c         Phase 1: Boot-Blink (PC13) + UART-Debug
    ├── bsp.h          Pin-Map (vorläufig) + BSP-API
    └── bsp.c          RCC/GPIO/UART-Init, printf-Retarget
```

## Build & Flash

```bash
cd firmware
pio run            # Build
pio run -t upload  # Flash über ST-Link
pio device monitor # UART-Debug (115200)
```

VS Code: PlatformIO-Extension öffnen, `firmware/` als Workspace laden.

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
- [ ] CubeMX/STM32CubeIDE-Projekt für Clock-Tree + Peripherie generieren
      (oder HAL-Init manuell in bsp.c ausbauen)
- [ ] ESP32-S3-Protokoll definieren (UART, Frames)
- [ ] FreeRTOS-Entscheidung: rein HAL+DMA-ISR oder Tasks für UI/Encoder

## Hinweis

Pin-Belegungen in `bsp.h` sind **vorläufig** (aus docs/architecture.md).
Bevor die Breadboard-Phase startet, müssen I2S/UART/Pinouts gegen das
tatsächliche NUCLEO-F767ZI-Board verifiziert werden.
