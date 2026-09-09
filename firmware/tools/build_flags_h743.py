"""Keep the linker ABI and C library consistent with the H743 compile flags.

The H743 (Cortex-M7) uses the double-precision FPU. We force hard-float with
fpv5-d16 on both compile and link so the HAL/CMSIS math is consistent.
"""
Import("env")
env.Append(LINKFLAGS=[
    "-mfpu=fpv5-d16",
    "-mfloat-abi=hard",
    "--specs=nano.specs",
    "--specs=nosys.specs",
])
