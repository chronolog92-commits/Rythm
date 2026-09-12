/**
 * RHYTHM – Driftfreies Perioden-Helfer (header-only)
 *
 * periodic_due() entscheidet anhand der VERSTRICHENEN Zeit (HAL_GetTick),
 * ob eine Periode fällig ist, statt Delay-Aufrufe zu zählen. Dadurch
 * akkumuliert sich keine Schleifen-Quantisierung mehr (z.B. die ~1 ms
 * Mindestwartezeit von HAL_Delay), und die Soll-Periode bleibt stabil.
 *
 * WICHTIG: Nach einem fälligen Takt wird `*last` um exakt `period_ms`
 * VERSCHOBEN (nicht auf `now` gesetzt). So verschiebt sich die nächste
 * Deadline nicht um die eigentliche Verspätung (kein Drift). Ein
 * verspäteter Aufruf (z.B. nach Debugger-Pause) erzeugt höchstens eine
 * einzelne Nachhol-Triggerung im nächsten Durchlauf — kein Stakkato,
 * aber auch keine stille Verschiebung.
 *
 * uint32_t-Overflow ist durch die Subtraktion (now - *last) sicher:
 * der Abstand wird als uint32_t gerechnet und überschreitet period_ms
 * (klein) nur bei echten, großen Abständen.
 */
#ifndef RHYTHM_PERIODIC_H
#define RHYTHM_PERIODIC_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Ist die Periode ab `*last` gegenüber `now` fällig?
 * @param now       aktueller Tick (z.B. HAL_GetTick())
 * @param last      Zeiger auf das letzte fällige Zeitstempel (in/out)
 * @param period_ms gewünschte Periode in ms
 * @return true, genau wenn die Perife fällig ist (und `*last` wird dann
 *         um period_ms verschoben).
 *
 * @note period_ms == 0 => immer false (keine Periode, sicher gegen Division).
 */
static inline bool periodic_due(uint32_t now, uint32_t *last, uint32_t period_ms)
{
    if (period_ms == 0U || (uint32_t)(now - *last) < period_ms) {
        return false;
    }
    *last += period_ms;
    return true;
}

#endif /* RHYTHM_PERIODIC_H */
