# Rythm Firmware — NUCLEO-H753ZI

Alle projektspezifischen C-/Assembler-Quellen, Header, HAL/CMSIS-Bausteine,
Startup-Code, Linkerskript und Build-Einstellungen liegen in diesem Ordner.
Es sind keine CubeIDE-Dateien, Vendor-Importe oder globalen PlatformIO-Patches
notwendig. Der Build ist jetzt für **STM32H753** (NUCLEO-H753ZI) — das aktuelle
und einzige Dev-Board (vorhanden). Der finale H7-Teil bleibt **STM32H743**;
bis das Ziel-Hardware da ist, entwickelt man ausschließlich auf dem H753ZI.

## Audio-Zielarchitektur (Stand 2026-09-12)

Die Projektdokumentation plant jetzt **7 Stereo-Eingänge und 5 unabhängige Stereo-Ausgänge**:
Stereo 1/2 über **1× ES9842PRO**, AUX über **1× ES9822PRO**, zusätzlich TOSLINK,
BT, WiFi und **USB-Audio vom PC (STM32 als Device)**. Kein analoger Quell-MUX.
Ausgänge: Stereo, AUX, Kopfhörer (drei getrennte DAC-Pfade), TOSLINK und BT.

Das ist eine Zielanforderung, **nicht der Funktionsumfang dieses Boot-/Clock-Gerüsts**.
SAI/TDM, SPDIFRX, SAI-S/PDIF-TX, USB-Device-Audio/Feedback, SRC, Mixer/EQ und
vollständige DMA-/Cache-Behandlung müssen noch implementiert und getestet werden.
ESP32-S3 ist nur WiFi-Kandidat; A2DP-Sink/-Source benötigen Classic-BT-Hardware.

**Clock-Blocker:** Der bei der Architekturprüfung vorgefundene PLL1-Entwurf in
`src/board_clock.c` nutzt HSI 64 MHz mit M=4, N=100 und P=2. Das ergibt rechnerisch
800 MHz statt 400 MHz; CPU-/Bus-/VCO-Grenzen sind vor weiterer Audiofreigabe zu
korrigieren und zu verifizieren. Es erfolgte in dieser Dokumentationsänderung
keine Firmwareänderung und kein Flashen. Ein erfolgreicher Build wäre hierfür
kein Hardware-Nachweis. Siehe [Clock-Warnhinweis](docs/clock-verification.md).

Details: [Architektur](../docs/architecture.md),
[Signalpfad](../docs/hardware/signal-path.md),
[Controller-Kapazitätsprüfung](../docs/hardware/controller-capacity.md).

## Boards & Environments

| Environment | Board | MCU | Zweck |
|-------------|-------|-----|-------|
| `nucleo-h753zi` (Default) | ST NUCLEO-H753ZI | STM32H753 | **Aktuelles Dev-Board** (vorhanden) |
| `nucleo-h753zi-debug` | ST NUCLEO-H753ZI | STM32H753 | Debug-Build (ST-Link/OpenOCD) |

> **Hinweis:** Die Firmware-Quellen (`src/`) sind H7-familien-agnostisch.
> `main.h` wählt je nach definiertem STM32-Teil (`STM32H7xx`) die passenden
> CMSIS-/HAL-Header. H753 und H743 teilen sich die STM32H7xx-HAL/CMSIS-Familie
> (identische HAL-Header, Cortex-M7); nur das Teil-Makro (`STM32H753xx` vs.
> `STM32H743xx`), die Startup-Vektortabelle und das Linkerskript unterscheiden
> sich. So bauen dieselben App-Quellen später auch für das H743-Ziel-Board.

## Voraussetzungen und Build

Extern erforderlich: Python und PlatformIO Core. PlatformIO installiert die in
`platformio.ini` festgelegte ST-STM32-Plattform 20.0.0, die ARM-GCC-Toolchain
7.2.1 (Paket 1.70201.0) und seine Build-Werkzeuge. Für deren Erstinstallation
ist Internetzugriff erforderlich; die Compiler-/C-Laufzeitbibliotheken werden
nicht als Projektquellen eingecheckt. Getestet unter Windows.

Das VS Code-Projekt ist im Repository-Root `rhythm/` geöffnet; PlatformIO
verwendet `rhythm/firmware/` als Projekt- und Quellverzeichnis:

```text
platformio.projectDir = ${workspaceFolder}/firmware
```

Build-Befehle laufen deshalb weiterhin aus `rhythm/firmware/`:

```text
pio run -e nucleo-h753zi
pio run -e nucleo-h753zi-debug
```

Ein sauberer Neubau:

```text
pio run -t clean -e nucleo-h753zi -e nucleo-h753zi-debug
pio run -e nucleo-h753zi -e nucleo-h753zi-debug
```

ELF/BIN entstehen unter `rhythm/firmware/.pio/build/<environment>/`.
`.pio/` wird nicht versioniert und kann nach dem Build gelöscht werden.

## Inhalt

- `src/`: Anwendung und Boardinitialisierung (bsp.c/bsp.h, main.c/main.h).
  Blaue LED, HAL_Init und SysTick_Handler für die HAL-Zeitbasis. Die Pin-Map
  in `bsp.h` ist vorläufig und muss mit dem NUCLEO-H753ZI-Pinout abgeglichen
  werden.
- `vendor_h753/`: CMSIS-Core, CMSIS-Device (H753), HAL (nur aktivierte Module),
  Startup (`startup_stm32h753xx.s`) und Linkerskript (`STM32H753ZITX_FLASH.ld`)
  für das aktuelle Dev-Board.
- `tools/build_flags_h753.py`: setzt Hard-Float-ABI (fpv5-d16) und
  Newlib-Nano/Nosys auch für den Linker. Normales lokales
  PlatformIO-Extra-Skript, kein Framework-Patch.

Die Vendor-Bausteine stammen aus dem offiziellen STM32CubeH7-Paket
(`framework-stm32cubeh7`). Unreferenzierte Module wurden weggelassen; bei neuen
Targets oder aktivierten HAL-Modulen müssen deren Quellen/Header bewusst
ergänzt werden. Die aktivierten HAL-Module (DMA, EXTI, FLASH, GPIO, I2C, I2S,
PWR, RCC, SPI, TIM, UART, CORTEX) bleiben unverändert.

## Flashen und Debuggen (optional, verändert den Boardzustand)

ST-Link und separat installiertes OpenOCD werden nur für Hardwarezugriff
benötigt, nicht zum Kompilieren. Nach einem erfolgreichen Debug-Build
(aktuelles H753-Board):

```text
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c "gdb port disabled" -c "tcl port disabled" -c "telnet port disabled" -c "program .pio/build/nucleo-h753zi-debug/firmware.elf verify reset exit"
```

Das ersetzt die Firmware. Nur mit passender Hardware und Freigabe ausführen.
Für Debugging muss das geladene ELF genau zur geflashten Firmware passen.
Beim alten ARM-GDB 8.0.50 funktioniert `target remote`; `extended-remote` kann
beim Trennen abstürzen. Nach dem Debuggen CPU-Laufzustand prüfen.

> **Wichtig (H7):** Der STM32H7 hat mehrere RAM-Bereiche (DTCM, AXI-SRAM,
> D2/D3). Das Linkerskript legt Code/Daten in `RAM_D1` (AXI-SRAM) an. Falsch
> adressierte RAM-Sektionen können zur Laufzeit stillschweigend fehlschlagen.

UART-Ausgabe über ST-Link COM ist nicht durch den LED-Test verifiziert.

## Drittanbieterquellen

Die Vendor-Dateien stammen aus dem zuvor importierten PlatformIO-Paket
`framework-stm32cubeh7`. Copyright- und Lizenzhinweise in den Dateien bleiben
unverändert. Die CMSIS-Core-Lizenz liegt in `vendor_h753/cmsis_core/LICENSE.txt`.
