/*
 * Host-Test für periodic_due() (src/periodic.h).
 *
 * Läuft auf dem HOST (kein MCU, kein HAL). Es wird NICHT die MCU-Firmware
 * gebaut — stattdessen wird derselbe Header gegen stdlib kompiliert und die
 * eigentliche Logik geprüft (kein dupliziertes Modell).
 *
 * Build (Host):  gcc -I../../src test_periodic.c -o test_periodic
 * Run:           ./test_periodic   -> exit 0 bei Erfolg
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "periodic.h"

static int failures = 0;
#define EXPECT(cond, msg) do { \
    if (!(cond)) { printf("  [FAIL] %s\n", msg); failures++; } \
    else { printf("  [ok]   %s\n", msg); } \
} while (0)

int main(void)
{
    const uint32_t period = 10000U;

    /* 1. Vor der Deadline: false, *last unverändert. */
    {
        uint32_t last = 0;
        EXPECT(!periodic_due(0, &last, period), "t=0 (== last): nicht fällig");
        EXPECT(!periodic_due(9999, &last, period), "t=9999 (< 10000): nicht fällig");
        EXPECT(last == 0, "last bleibt 0, wenn nicht fällig");
    }

    /* 2. Exakt an der Deadline: true, *last -> period. */
    {
        uint32_t last = 0;
        EXPECT(periodic_due(10000, &last, period), "t=10000 (== period): fällig");
        EXPECT(last == period, "last = period nach Trigger");
    }

    /* 3. Danach kein Doppelereignis im selben Durchlauf. */
    {
        uint32_t last = period; /* schon einmal ausgelöst */
        EXPECT(!periodic_due(10001, &last, period), "t=10001 (1ms nach 10s): NICHT doppelt");
    }

    /* 4. Verspäteter Aufruf: eine Triggerung, nächste Deadline NICHT verschoben. */
    {
        uint32_t last = 0;
        /* t=10050 (50ms Verspätung): fällig, last wird auf 10000 (NUR period) gesetzt */
        EXPECT(periodic_due(10050, &last, period), "t=10050 (verspätet): fällig");
        EXPECT(last == period, "last=10000 (period), NICHT 10050 -> kein Drift");
        /* Nächstes Mal erst bei 20000, nicht bei 10050+10000 */
        EXPECT(!periodic_due(19999, &last, period), "t=19999: noch nicht (Deadline 20000)");
        EXPECT(periodic_due(20000, &last, period), "t=20000: fällig (Soll-Periode 20s)");
        EXPECT(last == 2 * period, "last=20000 nach zweitem Trigger");
    }

    /* 5. uint32_t-Wraparound: Abstand wird mod 2^32 korrekt. */
    {
        uint32_t last = UINT32_MAX - 5;
        uint32_t now = 3;   /* 5 (wrap) + 3 -> Abstand = 8 */
        EXPECT(!periodic_due(now, &last, period), "wrap t=3 (Abstand 8 < 10000): nicht fällig");
        /* jetzt Abstand = 10000 -> fällig */
        now = 9998;
        EXPECT(periodic_due(now, &last, period), "wrap: Abstand 10000 -> fällig");
    }

    /* 6. period_ms == 0: immer false (kein Crash). */
    {
        uint32_t last = 0;
        EXPECT(!periodic_due(0, &last, 0), "period=0, t=0: false");
        EXPECT(!periodic_due(1000000, &last, 0), "period=0, t=1e6: false (kein Crash)");
        EXPECT(last == 0, "last unverändert bei period=0");
    }

    /* 7. Großes now (normale fortlaufende Ticks), mehrere Perioden. */
    {
        uint32_t last = 0;
        int triggers = 0;
        for (uint32_t t = 0; t < 25000; t++) {
            if (periodic_due(t, &last, period)) triggers++;
        }
        EXPECT(triggers == 2, "25000ms @10s Periode -> genau 2 Trigger");
        EXPECT(last == 2 * period, "last=20000");
    }

    printf("\n%s: %d Fehler\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
