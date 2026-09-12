/**
 * RHYTHM – BSP Implementation
 * Phase 1: minimale Initialisierung (Clock, GPIO, SysTick-Delay, UART-Debug).
 * I2S/DMA, Timer-Encoder, I2C, S/PDIF folgen in späteren Phasen.
 */

#include "bsp.h"

#include "stm32h7xx_hal.h"
#include "board_clock.h"
#include <stdio.h>
#include <string.h>

/* UART-Debug über USART2 (115200) */
static UART_HandleTypeDef huart_dbg;

void bsp_init(void)
{
    /* HAL_Delay needs the 1 ms SysTick timebase before peripheral init. */
    HAL_Init();

    /* System-Clock auf 400 MHz (VOS1, HSI-PLL1). MUSS vor der Peripherie-Init
     * laufen, damit GPIO/UART mit dem finalen Takt konfiguriert werden. Bei
     * Fehlschlag bleibt der Chip sicher auf HSI 64 MHz. */
    const int clock_ok = board_clock_init();
    (void)clock_ok; /* Fehler wird in g_board_clock_error kodiert */

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();   /* PE1: gelbe User-LED LD2 (USER_LED2), aktiv-high. */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    /* Blaue LED LD2 an PB7 (NUCLEO-H753ZI / H743ZI identisch), nicht USER-Taster PC13. */
    GPIO_InitTypeDef led = {0};
    led.Pin   = BSP_LED_PIN;
    led.Mode  = GPIO_MODE_OUTPUT_PP;
    led.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BSP_LED_PORT, &led);
    HAL_GPIO_WritePin(BSP_LED_PORT, BSP_LED_PIN, GPIO_PIN_SET);

    /* UART-Debug (USART2, 115200 8N1) – retarget printf */
    huart_dbg.Instance          = USART2;
    huart_dbg.Init.BaudRate     = ESP_UART_BAUD;
    huart_dbg.Init.WordLength   = UART_WORDLENGTH_8B;
    huart_dbg.Init.StopBits     = UART_STOPBITS_1;
    huart_dbg.Init.Parity       = UART_PARITY_NONE;
    huart_dbg.Init.Mode         = UART_MODE_TX_RX;
    huart_dbg.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart_dbg.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart_dbg);
}

/* Override the startup file's weak default handler for the HAL timebase. */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

void bsp_toggle_led(void)
{
    HAL_GPIO_TogglePin(BSP_LED_PORT, BSP_LED_PIN);
}

void bsp_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/* ---------- printf → USART2 ---------- */
int _write(int file, char *ptr, int len)
{
    (void)file;
    HAL_UART_Transmit(&huart_dbg, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}
