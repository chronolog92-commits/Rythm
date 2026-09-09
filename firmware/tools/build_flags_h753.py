"""Keep the linker ABI and C library consistent with the H753 compile flags.

The H753 (Cortex-M7) uses the double-precision FPU, like the H743. We force
hard-float with fpv5-d16 on both compile and link so the HAL/CMSIS math is
consistent. The H753 shares the STM32H7xx HAL/CMSIS family with the H743; only
the part define (STM32H753xx) and the startup vector table / linker script
differ.
"""
Import("env")
env.Append(LINKFLAGS=[
    "-mfpu=fpv5-d16",
    "-mfloat-abi=hard",
    "--specs=nano.specs",
    "--specs=nosys.specs",
])
