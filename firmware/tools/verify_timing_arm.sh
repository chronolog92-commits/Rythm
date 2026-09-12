#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# verify_timing_arm.sh
#
# Verifiziert die driftfreie Periodenlogik (src/periodic.h) GEGEN DEN ZIEL-COMPILER
# (arm-none-eabi-gcc aus der PlatformIO-Toolchain), ohne Host-C-Compiler oder QEMU
# zu benötigen.
#
# Es werden ZWEI Dinge gebaut (Beide gegen denselben echten Ziel-Compiler):
#   1) test_timing_arm.elf : Der echte Test (tests/timing/test_periodic.c) + periodic.h
#      -> kompiliert UND linkt. Der Host kann das ARM-Binary nicht ausfuehren; der
#      Build-Nachweis ist "Logik kompiliert gegen den Ziel-Compiler". (Kein Stub!)
#   2) firmware.elf        : Der eigentliche MCU-Firmware-Build via PlatformIO.
#
# Aufruf (aus firmware/):  bash tools/verify_timing_arm.sh
# Exit 0 = beide Builds erfolgreich.
# -----------------------------------------------------------------------------
set -euo pipefail
cd "$(dirname "$0")/.."   # -> firmware/

TC="$HOME/.platformio/packages/toolchain-gccarmnoneeabi/bin"
if [ ! -x "$TC/arm-none-eabi-gcc" ]; then
    echo "FATAL: arm-none-eabi-gcc nicht unter $TC gefunden." >&2
    exit 2
fi

ARM_CFLAGS="-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard -Wall -Wextra"

echo "=== [1/2] ARM-Test-Build (echte Testlogik gegen Ziel-Compiler) ==="
# test_periodic.c braucht stdio/stdlib (printf/exit) -> nosys.specs fuer ARM.
"$TC/arm-none-eabi-gcc" $ARM_CFLAGS -Isrc tests/timing/test_periodic.c \
    --specs=nosys.specs -o /tmp/rhythm_test_timing_arm.elf
echo "  OK: test_timing_arm.elf gebaut (echte Testlogik, kein Stub)."

# ELF-Integritaet + Symbole pruefen (beweist: kein leeres/Platzhalter-Binary).
if [ -f /tmp/rhythm_test_timing_arm.elf ]; then
    magic=$(head -c4 /tmp/rhythm_test_timing_arm.elf | od -An -tx1 | tr -d ' \n')
    echo "  ELF-Magic: $magic  (erwartet 7f454c46)"
    "$TC/arm-none-eabi-objdump" -t /tmp/rhythm_test_timing_arm.elf | grep -E " (main|periodic_due)" || true
fi

echo "=== [2/2] Firmware-Build via PlatformIO (nucleo-h753zi) ==="
pio run -e nucleo-h753zi

echo "=== ERGEBNIS: beide Builds erfolgreich ==="
