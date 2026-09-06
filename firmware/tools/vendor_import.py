#!/usr/bin/env python3
"""Vendor: HAL + CMSIS + Startup + Linker-Skript ins Repo kopieren.

Quelle: PlatformIO-Paket framework-stm32cubef7 (ststm32-Plattform)
  + tool-ldscripts-ststm32 (Linker-Skript STM32F767ZI).
Ziel:   firmware/vendor/ (repo-intern, kein Cache-Abhaengigkeit mehr).

Nur die HAL-Module, die RHYTHM braucht:
  HAL, CORTEX, GPIO, DMA, RCC, PWR, EXTI, FLASH, I2C, I2S, SPI, TIM, UART
  (SPL-I2S-Mode wird ueber -DSPL_I2S_DRIVER im Build gesetzt.)
"""
import shutil
import sys
from pathlib import Path

F7 = Path.home() / ".platformio/packages/framework-stm32cubef7/Drivers"
LDS = Path.home() / ".platformio/packages/tool-ldscripts-ststm32/stm32f7"
DEST = Path(__file__).resolve().parent.parent / "vendor"

# (relativ zu F7, Ziel-Unterordner)
FILES = [
    # CMSIS-Core (alle Header, kompakt)
    ("CMSIS/Core/Include", "cmsis_core"),
    # CMSIS-Device STM32F7xx (alle Header + system_stm32f7xx.c)
    ("CMSIS/Device/ST/STM32F7xx", "cmsis_device"),
    # GCC-Startup
    ("CMSIS/Device/ST/STM32F7xx/Source/Templates/gcc/startup_stm32f767xx.s", "startup"),
    # Linker-Skript (NUCLEO-F767ZI = 2MB Flash, 512KB SRAM + 256KB CCM)
    ("../..", "linker"),  # placeholder, wird unten behandelt
]

HAL_MODULES = {
    "stm32f7xx_hal.c", "stm32f7xx_hal.h", "stm32f7xx_hal_def.h",
    "stm32f7xx_hal_conf.h", "stm32f7xx_hal_conf_template.h",
    "stm32_assert_template.h",
    "stm32f7xx_hal_cortex.c", "stm32f7xx_hal_cortex.h",
    "stm32f7xx_hal_gpio.c", "stm32f7xx_hal_gpio.h", "stm32f7xx_hal_gpio_ex.h",
    "stm32f7xx_hal_dma.c", "stm32f7xx_hal_dma.h", "stm32f7xx_hal_dma_ex.c", "stm32f7xx_hal_dma_ex.h",
    "stm32f7xx_hal_rcc.c", "stm32f7xx_hal_rcc.h", "stm32f7xx_hal_rcc_ex.c", "stm32f7xx_hal_rcc_ex.h",
    "stm32f7xx_hal_pwr.c", "stm32f7xx_hal_pwr.h", "stm32f7xx_hal_pwr_ex.c", "stm32f7xx_hal_pwr_ex.h",
    "stm32f7xx_hal_exti.c", "stm32f7xx_hal_exti.h",
    "stm32f7xx_hal_flash.c", "stm32f7xx_hal_flash.h", "stm32f7xx_hal_flash_ex.c", "stm32f7xx_hal_flash_ex.h",
    "stm32f7xx_hal_i2c.c", "stm32f7xx_hal_i2c.h", "stm32f7xx_hal_i2c_ex.c", "stm32f7xx_hal_i2c_ex.h",
    "stm32f7xx_hal_i2s.c", "stm32f7xx_hal_i2s.h",
    "stm32f7xx_hal_spi.c", "stm32f7xx_hal_spi.h", "stm32f7xx_hal_spi_ex.c", "stm32f7xx_hal_spi_ex.h",
    "stm32f7xx_hal_tim.c", "stm32f7xx_hal_tim.h", "stm32f7xx_hal_tim_ex.c", "stm32f7xx_hal_tim_ex.h",
    "stm32f7xx_hal_uart.c", "stm32f7xx_hal_uart.h", "stm32f7xx_hal_uart_ex.c", "stm32f7xx_hal_uart_ex.h",
}

# Inc/Legacy (wird von stm32f7xx_hal_def.h eingebunden)
HAL_LEGACY_HEADERS = {"stm32_hal_legacy.h"}


def copy_dir(src: Path, dst: Path) -> int:
    dst.mkdir(parents=True, exist_ok=True)
    n = 0
    for f in sorted(src.rglob("*")):
        if f.is_file():
            rel = f.relative_to(src)
            (dst / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(f, dst / rel)
            n += 1
    return n


def trim_hal_conf(conf: Path) -> None:
    """hal_conf.h: Module deaktivieren, die wir nicht vendored haben.

    Das Paket-Default hat ALLE Module aktiv (inkl. Legacy-Header-Pfade wie
    stm32f7xx_hal_adc.h, die wir bewusst nicht kopiert haben). Wir setzen
    nur die Module auf ENABLED, deren Quellen wir in vendor/hal/ haben.
    """
    keep = {
        "HAL_MODULE_ENABLED",
        "HAL_DMA_MODULE_ENABLED",
        "HAL_EXTI_MODULE_ENABLED",
        "HAL_FLASH_MODULE_ENABLED",
        "HAL_GPIO_MODULE_ENABLED",
        "HAL_I2C_MODULE_ENABLED",
        "HAL_I2S_MODULE_ENABLED",
        "HAL_PWR_MODULE_ENABLED",
        "HAL_RCC_MODULE_ENABLED",
        "HAL_SPI_MODULE_ENABLED",
        "HAL_TIM_MODULE_ENABLED",
        "HAL_UART_MODULE_ENABLED",
        "HAL_CORTEX_MODULE_ENABLED",
    }
    text = conf.read_text(encoding="utf-8")
    lines = text.splitlines(keepends=True)
    out = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("#define HAL_") and "_MODULE_ENABLED" in stripped:
            # Modulname extrahieren (z.B. HAL_ADC_MODULE_ENABLED)
            mod = stripped.split()[1]
            if mod not in keep:
                # Kommentarieren: "#define HAL_ADC_MODULE_ENABLED" ->
                # "/* #define HAL_ADC_MODULE_ENABLED */"
                out.append(f"/* {line}*/" if not line.endswith("\n") else f"/* {line}*/\n")
                continue
        out.append(line)
    conf.write_text("".join(out), encoding="utf-8")


def main() -> int:
    if not F7.is_dir():
        print(f"FATAL: Framework-Paket nicht gefunden: {F7}", file=sys.stderr)
        return 1
    if not LDS.is_dir():
        print(f"FATAL: Linker-Skripte nicht gefunden: {LDS}", file=sys.stderr)
        return 1

    # 1) CMSIS-Core
    n = copy_dir(F7 / "CMSIS/Core/Include", DEST / "cmsis_core")
    print(f"cmsis_core: {n} Dateien")

    # 2) CMSIS-Device (Header + system_stm32f7xx.c, Templates/gcc aussen)
    dev = F7 / "CMSIS/Device/ST/STM32F7xx"
    n = 0
    for f in sorted(dev.rglob("*")):
        if f.is_file() and "Templates" not in f.parts:
            rel = f.relative_to(dev)
            (DEST / "cmsis_device" / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(f, DEST / "cmsis_device" / rel)
            n += 1
    print(f"cmsis_device: {n} Dateien")

    # 3) HAL-Driver (nur gewaehlte Module)
    inc = F7 / "STM32F7xx_HAL_Driver/Inc"
    src = F7 / "STM32F7xx_HAL_Driver/Src"
    for name in sorted(HAL_MODULES):
        for s, d in ((inc, "hal/Inc"), (src, "hal/Src")):
            f = s / name
            if f.is_file():
                (DEST / d).mkdir(parents=True, exist_ok=True)
                shutil.copy2(f, DEST / d / name)
    # Legacy-Header (stm32_hal_legacy.h wird von hal_def.h eingebunden)
    leg = F7 / "STM32F7xx_HAL_Driver/Inc/Legacy"
    for name in sorted(HAL_LEGACY_HEADERS):
        f = leg / name
        if f.is_file():
            (DEST / "hal/Inc/Legacy").mkdir(parents=True, exist_ok=True)
            shutil.copy2(f, DEST / "hal/Inc/Legacy" / name)
    # hal_conf.h auf die vendored Module trimmen
    trim_hal_conf(DEST / "hal/Inc/stm32f7xx_hal_conf.h")
    print(f"hal: {len(list((DEST/'hal/Inc').glob('stm32f7xx*.h')))} Header, "
          f"{len(list((DEST/'hal/Src').glob('*'))) } Quellen")

    # 4) Startup
    s = F7 / "CMSIS/Device/ST/STM32F7xx/Source/Templates/gcc/startup_stm32f767xx.s"
    (DEST / "startup").mkdir(parents=True, exist_ok=True)
    shutil.copy2(s, DEST / "startup/startup_stm32f767xx.s")
    print("startup: 1 Datei")

    # 5) Linker-Skript
    l = LDS / "STM32F767ZITX_FLASH.ld"
    if not l.is_file():
        print(f"FATAL: Linker-Skript fehlt: {l}", file=sys.stderr)
        return 1
    (DEST / "linker").mkdir(parents=True, exist_ok=True)
    shutil.copy2(l, DEST / "linker/STM32F767ZI_FLASH.ld")
    print("linker: 1 Datei")

    print(f"\nOK: vendor/ unter {DEST}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
