/**
 * RHYTHM – Main
 * STM32H753 (NUCLEO-H753ZI als Dev-Board; final STM32H743)
 *
 * Phase 1 (Breadboard): Blink-Loop auf blauer LED LD2 (PB7) + UART-Debug.
 * Audio-Stack (I2S/DMA, EQ, Mixing) kommt in den folgenden Phasen.
 */

#include "main.h"
#include "bsp.h"
#include "periodic.h"
#include <stdio.h>

int main(void)
{
    bsp_init();

    printf("RHYTHM firmware started (phase: boot)\r\n");

    /* Gelbe LED (PE1) alle 10 s toggeln.
     * Zeitbasiert über HAL_GetTick()/periodic_due() statt Delay-Aufrufe zu
     * zaehlen: so akkumuliert sich keine Schleifen-Quantisierung mehr und die
     * 10-s-Sollperiode bleibt stabil (kein Drift). bsp_delay_ms(1) dient nur
     * als kurzes Polling, damit der Tick-Treiber zeitnah reagiert. */
    uint32_t last_led_ms = HAL_GetTick();
    while (1)
    {
        const uint32_t now = HAL_GetTick();
        if (periodic_due(now, &last_led_ms, 10000U))
        {
            bsp_toggle_led();
        }
        bsp_delay_ms(1);
    }

    return 0;
}
