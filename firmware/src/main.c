/**
 * RHYTHM – Main
 * STM32H743 (NUCLEO-H743ZI als Dev-Board)
 *
 * Phase 1 (Breadboard): Blink-Loop auf blauer LED LD2 (PB7) + UART-Debug.
 * Audio-Stack (I2S/DMA, EQ, Mixing) kommt in den folgenden Phasen.
 */

#include "main.h"
#include "bsp.h"
#include <stdio.h>

int main(void)
{
    bsp_init();

    printf("RHYTHM firmware started (phase: boot)\r\n");

    uint32_t ticks = 0;
    while (1)
    {
        ticks++;
        if ((ticks % 200) == 0)
        {
            bsp_toggle_led();
        }
        bsp_delay_ms(10);
    }

    return 0;
}
