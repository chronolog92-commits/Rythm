# RHYTHM: Bare-Metal mit repo-internem HAL (vendor/).
#
# Wirft NICHTS nach: HAL-, CMSIS-, Startup- und Linker-Skript kommen alle aus
# vendor/ (siehe tools/vendor_import.py).

import os

from SCons.Script import DefaultEnvironment

env = DefaultEnvironment()
platform = env.PioPlatform()
board = env.BoardConfig()

MCU = board.get("build.mcu", "")
PROJECT_DIR = env.subst("$PROJECT_DIR")
VENDOR = os.path.join(PROJECT_DIR, "vendor")

# Sanity-Check: vendor/ muss vorhanden sein
required = [
    "cmsis_core",
    "cmsis_device/Include",
    "cmsis_device/Source",
    "hal/Inc",
    "hal/Src",
    "linker",
    "startup",
]
missing = [d for d in required if not os.path.isdir(os.path.join(VENDOR, d))]
if missing:
    raise AssertionError(
        "vendor/ unvollstaendig (fehlt: %s). "
        "Zuerst ausfuehren: python tools/vendor_import.py" % ", ".join(missing)
    )

env.Append(
    CPPDEFINES=[
        "USE_HAL_DRIVER",
        ("F_CPU", "$BOARD_F_CPU"),
    ],
    CPPPATH=[
        os.path.join(VENDOR, "cmsis_core"),
        os.path.join(VENDOR, "cmsis_device", "Include"),
        os.path.join(VENDOR, "hal", "Inc"),
    ],
)

# CPU-Flags: explizit Cortex-M7 (BuildLibrary erbt diese CCFLAGS/ASFLAGS/LINKFLAGS)
env.Append(
    CCFLAGS=[
        "-mthumb",
        "-mcpu=cortex-m7",
        "-mfpu=fpv5-d16",
        "-mfloat-abi=hard",
    ],
    CXXFLAGS=[
        "-mthumb",
        "-mcpu=cortex-m7",
        "-mfpu=fpv5-d16",
        "-mfloat-abi=hard",
    ],
    ASFLAGS=[
        "-mthumb",
        "-mcpu=cortex-m7",
    ],
    LINKFLAGS=[
        "-mthumb",
        "-mcpu=cortex-m7",
        "-mfpu=fpv5-d16",
        "-mfloat-abi=hard",
        "--specs=nano.specs",
        "--specs=nosys.specs",
    ],
)

# HAL-Module + CMSIS-System + GCC-Startup: explizit als Bibliotheken bauen.
# BuildLibrary erbt die oben gesetzten CCFLAGS (Cortex-M7).
hal_lib = env.BuildLibrary(
    os.path.join("$BUILD_DIR", "FrameworkHAL"),
    os.path.join(VENDOR, "hal", "Src"),
    src_filter="+<*> -<*_template.c>",
)
cmsis_lib = env.BuildLibrary(
    os.path.join("$BUILD_DIR", "FrameworkCMSIS"),
    os.path.join(VENDOR, "cmsis_device", "Source"),
    src_filter=[
        "-<*>",
        "+<Templates/system_stm32f7xx.c>",
    ],
)
# GCC-Startup (Reset_Handler, vector table)
startup_lib = env.BuildLibrary(
    os.path.join("$BUILD_DIR", "FrameworkStartup"),
    os.path.join(VENDOR, "startup"),
    src_filter="+<*.s>",
)
env.Append(LIBS=[hal_lib, cmsis_lib, startup_lib])
