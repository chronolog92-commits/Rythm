# Rythm Firmware — NUCLEO-H743ZI

Alle projektspezifischen C-/Assembler-Quellen, Header, HAL/CMSIS-Bausteine,
Startup-Code, Linkerskript und Build-Einstellungen liegen in diesem Ordner.
Es sind keine CubeIDE-Dateien, Vendor-Importe oder globalen PlatformIO-Patches
notwendig. Der Build ist jetzt für **STM32H743** (NUCLEO-H743ZI) — das
dokumentierte finale H7-Zielboard. Der frühere **STM32F767** (NUCLEO-F767ZI)
bleibt als Fallback-Environment erhalten.

## Boards & Environments

| Environment | Board | MCU | Zweck |
|-------------|-------|-----|-------|
| `nucleo-h743zi` (Default) | ST NUCLEO-H743ZI | STM32H743 | **Aktuelles Dev-Board** (neu) |
| `nucleo-h743zi-debug` | ST NUCLEO-H743ZI | STM32H743 | Debug-Build (ST-Link/OpenOCD) |
| `nucleo-f767zi` | ST NUCLEO-F767ZI | STM32F767 | Fallback (früheres Dev-Board) |
| `nucleo-f767zi-debug` | ST NUCLEO-F767ZI | STM32F767 | Debug-Build (Fallback) |

> **Hinweis:** Die Firmware-Quellen (`src/`) sind board-agnostisch.
> `main.h` wählt je nach definiertem STM32-Teil (`STM32H7xx` vs. `STM32F767xx`)
> die passenden CMSIS-/HAL-Header. So bauen dieselben App-Quellen für beide Boards.

## Voraussetzungen und Build

Extern erforderlich: Python und PlatformIO Core. PlatformIO installiert die in
`platformio.ini` festgelegte ST-STM32-Plattform 20.0.0, die ARM-GCC-Toolchain
7.2.1 (Paket 1.70201.0) und seine Build-Werkzeuge. Für deren Erstinstallation
ist Internetzugriff erforderlich; die Compiler-/C-Laufzeitbibliotheken werden
nicht als Projektquellen eingecheckt. Getestet unter Windows.

Aus diesem Ordner (Standard = neues H743-Board):

```text
pio run -e nucleo-h743zi
pio run -e nucleo-h743zi-debug
```

Fallback (F767):

```text
pio run -e nucleo-f767zi
pio run -e nucleo-f767zi-debug
```

Ein sauberer Neubau:

```text
pio run -t clean -e nucleo-h743zi -e nucleo-h743zi-debug
pio run -e nucleo-h743zi -e nucleo-h743zi-debug
```

ELF/BIN entstehen unter `.pio/build/<environment>/`. `.pio/` wird nicht
versioniert und kann nach dem Build geloescht werden.

## Inhalt

- `src/`: Anwendung und Boardinitialisierung (bsp.c/bsp.h, main.c/main.h).
  Blaue LED, HAL_Init und SysTick_Handler fuer die HAL-Zeitbasis. Die Pin-Map
  in `bsp.h` ist vorlaeufig und muss mit dem NUCLEO-H743ZI-Pinout abgeglichen
  werden (vorher F767).
- `vendor_h743/`: CMSIS-Core, CMSIS-Device (H743), HAL (nur aktivierte Module),
  Startup (`startup_stm32h743xx.s`) und Linkerskript (`STM32H743ZI_FLASH.ld`).
- `vendor_f767/`: derselbe Aufbau fuer das Fallback-Board (F767), unveraendert
  vom urspruenglichen Stand.
- `tools/build_flags_h743.py` / `tools/build_flags.py`: setzen Hard-Float-ABI
  (fpv5-d16) und Newlib-Nano/Nosys auch fuer den Linker. Normale lokale
  PlatformIO-Extra-Skripte, keine Framework-Patches.

Die Vendor-Bausteine stammen aus dem offiziellen STM32CubeH7-Paket
(`framework-stm32cubeh7`) bzw. frueher `framework-stm32cubef7`. Unreferenzierte
Module wurden weggelassen; bei neuen Targets oder aktivierten HAL-Modulen
muessen deren Quellen/Header bewusst ergaenzt werden. Die aktivierten
HAL-Module (DMA, EXTI, FLASH, GPIO, I2C, I2S, PWR, RCC, SPI, TIM, UART,
CORTEX) bleiben unveraendert.

## Flashen und Debuggen (optional, veraendert den Boardzustand)

ST-Link und separat installiertes OpenOCD werden nur fuer Hardwarezugriff
benoetigt, nicht zum Kompilieren. Nach einem erfolgreichen Debug-Build
(aktuelles H743-Board):

```text
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c "gdb port disabled" -c "tcl port disabled" -c "telnet port disabled" -c "program .pio/build/nucleo-h743zi-debug/firmware.elf verify reset exit"
```

Das ersetzt die Firmware. Nur mit passender Hardware und Freigabe ausfuehren.
Fuer Debugging muss das geladene ELF genau zur geflashten Firmware passen.
Beim alten ARM-GDB 8.0.50 funktioniert `target remote`; `extended-remote` kann
beim Trennen abstuerzen. Nach dem Debuggen CPU-Laufzustand pruefen.

> **Wichtig (H7):** Der STM32H7 hat mehrere RAM-Bereiche (DTCM, AXI-SRAM,
> D2/D3). Das Linkerskript legt Code/Daten in `RAM_D1` (AXI-SRAM) an. Falsch
> adressierte RAM-Sektionen koennen zur Laufzeit stillschweigend fehlschlagen.

UART-Ausgabe ueber ST-Link COM ist nicht durch den LED-Test verifiziert.

## Drittanbieterquellen

Die Vendor-Dateien stammen aus den zuvor importierten PlatformIO-Paketen
`framework-stm32cubeh7` (H743) und `framework-stm32cubef7` (F767).
Copyright- und Lizenzhinweise in den Dateien bleiben unveraendert. Die
CMSIS-Core-Lizenz liegt in `vendor_h743/cmsis_core/LICENSE.txt` bzw.
`vendor_f767/cmsis_core/LICENSE.txt`.
