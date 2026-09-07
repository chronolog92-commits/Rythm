# Vendor Licenses

All files in this directory are third-party components from the official
STM32CubeF7 software package (STMicroelectronics) and the CMSIS Core package
(ARM). They are provided under the licenses listed below.

Per their license terms, the copyright notices in the file headers must be
preserved when redistributing. The original license texts are included
alongside each component directory.

| Directory        | Component                                      | License        | Source |
|------------------|------------------------------------------------|----------------|--------|
| `cmsis_core/`    | ARM CMSIS Core (Cortex-M)                      | Apache-2.0     | https://github.com/STMicroelectronics/cmsis_core (mirror of ARM CMSIS) |
| `cmsis_device/`  | ST CMSIS Device F7 (device headers, system_*.c)| Apache-2.0     | https://github.com/STMicroelectronics/cmsis_device_f7 |
| `hal/`           | ST STM32F7xx HAL driver                        | BSD-3-Clause   | https://github.com/STMicroelectronics/stm32f7xx_hal_driver |
| `startup/`       | GCC startup file (startup_stm32f767xx.s)       | Apache-2.0     | https://github.com/STMicroelectronics/cmsis_device_f7 (Source/Templates/gcc/) |
| `linker/`        | GCC linker script (STM32F767ZI_FLASH.ld)       | BSD-3-Clause   | STM32CubeF7 package (license statement in file header) |

## Copyrights

- Copyright (c) 2009-2017, 2019-2023 Arm Limited. Apache-2.0.
- Copyright (c) 2010-2026 STMicroelectronics. All rights reserved.
  (HAL / CMSIS Device / Startup / Linker – see per-file headers and the
  per-component LICENSE files below.)

## Included license texts

- `cmsis_core/LICENSE.txt`      – Apache License 2.0 (CMSIS Core, ARM)
- `cmsis_device/LICENSE.md`     – Apache License 2.0 (CMSIS Device F7, ST)
- `hal/LICENSE.md`              – BSD 3-Clause "New" or "Revised" License (HAL, ST)
- `startup/LICENSE.md`          – Apache License 2.0 (Startup, ST)
- `linker/`                     – BSD-3-Clause (statement embedded in the .ld header)
