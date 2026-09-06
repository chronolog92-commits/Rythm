# Rythm Firmware — NUCLEO-F767ZI

Alle projektspezifischen C-/Assembler-Quellen, Header, HAL/CMSIS-Bausteine,
Startup-Code, Linkerskript und Build-Einstellungen liegen in diesem Ordner.
Es sind keine CubeIDE-Dateien, Vendor-Importe oder globalen PlatformIO-Patches
notwendig. Der aktuelle Build ist fuer STM32F767ZI, noch nicht fuer STM32H743.

## Voraussetzungen und Build

Extern erforderlich: Python und PlatformIO Core. PlatformIO installiert die in
`platformio.ini` festgelegte ST-STM32-Plattform 20.0.0, die ARM-GCC-Toolchain
7.2.1 (Paket 1.70201.0) und seine Build-Werkzeuge. Fuer deren Erstinstallation
ist Internetzugriff erforderlich; die Compiler-/C-Laufzeitbibliotheken werden
nicht als Projektquellen eingecheckt. Getestet unter Windows.

Aus diesem Ordner:

```text
pio run -e nucleo-f767zi
pio run -e nucleo-f767zi-debug
```

Ein sauberer Neubau:

```text
pio run -t clean -e nucleo-f767zi -e nucleo-f767zi-debug
pio run -e nucleo-f767zi -e nucleo-f767zi-debug
```

ELF/BIN entstehen unter `.pio/build/<environment>/`. `.pio/` wird nicht
versioniert und kann nach dem Build geloescht werden.

## Inhalt

- `src/`: Anwendung und Boardinitialisierung; blaue LED LD2 an PB7,
  HAL_Init und SysTick_Handler fuer die HAL-Zeitbasis.
- `vendor/cmsis_core/`: fuer Cortex-M7 / GCC benoetigte CMSIS-Core-Header.
- `vendor/cmsis_device/`: STM32F767-Header und Systeminitialisierung.
- `vendor/hal/`: Quellen/Header fuer die derzeit aktivierten HAL-Module.
- `vendor/startup/`: Interruptvektoren und Reset_Handler.
- `vendor/linker/`: STM32F767ZI-Speicherlayout.
- `tools/build_flags.py`: setzt Hard-Float-ABI und Newlib-Nano/Nosys auch
  fuer den Linker. Normales lokales PlatformIO-Extra-Skript, kein Framework-Patch.

Andere MCU-/Compiler-Header und unbenutzte Templates wurden anhand der
GCC-Include-Abhaengigkeiten entfernt. Bei neuen Targets oder aktivierten
HAL-Modulen muessen deren Quellen/Header bewusst ergaenzt werden.
Die aktivierten HAL-Module bleiben unveraendert; unreferenzierter Code wird
vom Linker entfernt. Weitere Pinzuordnungen in `bsp.h` sind vorlaeufig.

## Flashen und Debuggen (optional, veraendert den Boardzustand)

ST-Link und separat installiertes OpenOCD werden nur fuer Hardwarezugriff
benoetigt, nicht zum Kompilieren. Nach einem erfolgreichen Debug-Build:

```text
openocd -f interface/stlink.cfg -f target/stm32f7x.cfg -c "gdb port disabled" -c "tcl port disabled" -c "telnet port disabled" -c "program .pio/build/nucleo-f767zi-debug/firmware.elf verify reset exit"
```

Das ersetzt die Firmware. Nur mit passender Hardware und Freigabe ausfuehren.
Fuer Debugging muss das geladene ELF genau zur geflashten Firmware passen.
Beim alten ARM-GDB 8.0.50 funktioniert `target remote`; `extended-remote` kann
beim Trennen abstuerzen. Nach dem Debuggen CPU-Laufzustand pruefen.
UART-Ausgabe ueber ST-Link COM ist nicht durch den LED-Test verifiziert.

## Drittanbieterquellen

Die Vendor-Dateien stammen aus dem zuvor importierten PlatformIO-Paket
`framework-stm32cubef7`. Copyright- und Lizenzhinweise in den Dateien bleiben
unveraendert. Die CMSIS-Core-Lizenz liegt in `vendor/cmsis_core/LICENSE.txt`.
