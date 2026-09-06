"""Keep the linker ABI and C library consistent with the compile flags."""
Import("env")
env.Append(LINKFLAGS=[
    "-mfpu=fpv5-d16",
    "-mfloat-abi=hard",
    "--specs=nano.specs",
    "--specs=nosys.specs",
])
