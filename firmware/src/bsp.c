/**
 * RHYTHM – BSP Implementation
 * Phase 1: minimale Initialisierung (Clock, GPIO, SysTick-Delay, UART-Debug,
 * USB OTG_FS Full-Speed).
 * I2S/DMA, Timer-Encoder, I2C, S/PDIF folgen in späteren Phasen.
 */

#include "bsp.h"

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_gpio_ex.h"
#include "board_clock.h"
#include "tusb.h"          /* dcd_int_handler für OTG_FS_IRQHandler */
#include <stdio.h>
#include <string.h>

/* UART-Debug über USART3 (115200) */
static UART_HandleTypeDef huart_dbg;

void bsp_init(void)
{
    /* HAL_Delay needs the 1 ms SysTick timebase before peripheral init. */
    HAL_Init();

    /* System-Clock auf 480 MHz (VOS0, HSI-PLL1). MUSS vor der Peripherie-Init
     * laufen, damit GPIO/UART mit dem finalen Takt konfiguriert werden. Bei
     * Fehlschlag bleibt der Chip sicher auf HSI 64 MHz. */
    const int clock_ok = board_clock_init();
    (void)clock_ok; /* Fehler wird in g_board_clock_error kodiert */

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();   /* PE1: gelbe User-LED LD2 (USER_LED2), aktiv-high. */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();

    /* Blaue LED LD2 an PB7 (NUCLEO-H753ZI / H743ZI identisch), nicht USER-Taster PC13. */
    GPIO_InitTypeDef led = {0};
    led.Pin   = BSP_LED_PIN;
    led.Mode  = GPIO_MODE_OUTPUT_PP;
    led.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BSP_LED_PORT, &led);
    HAL_GPIO_WritePin(BSP_LED_PORT, BSP_LED_PIN, GPIO_PIN_SET);

    /* UART-Debug (USART3, 115200 8N1) – retarget printf.
     * TX = PD8, RX = PD9, beide AF7 (GPIO D-Clock ist oben bereits on). */
    huart_dbg.Instance          = USART3;
    huart_dbg.Init.BaudRate     = 115200;
    huart_dbg.Init.WordLength   = UART_WORDLENGTH_8B;
    huart_dbg.Init.StopBits     = UART_STOPBITS_1;
    huart_dbg.Init.Parity       = UART_PARITY_NONE;
    huart_dbg.Init.Mode         = UART_MODE_TX_RX;
    huart_dbg.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart_dbg.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart_dbg);

    /* USART3-GPIO: TX = PD8, RX = PD9, beide als Alternate Function AF7.
     * OHNE diese Init bleibt der Pin im GPIO-Input-Default und der
     * UART-TX kann den Pin nicht schalten -> kein Output auf der Leitung. */
    GPIO_InitTypeDef dbg = {0};
    dbg.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    dbg.Mode      = GPIO_MODE_AF_PP;
    dbg.Pull      = GPIO_PULLUP;
    dbg.Speed     = GPIO_SPEED_FREQ_HIGH;
    dbg.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &dbg);

    /* USB OTG FS – Full-Speed GPIO-Setup (PA11/PA12, interne FS-PHY).
     * MUSS vor tusb_init() laufen, damit die DM/DP-Pins als AF10 stehen. */
    bsp_usb_fs_init();
}

/* ---------- USB OTG FS – Full Speed (interne FS-PHY) ----------
 * PA11 = USB_DM, PA12 = USB_DP (USB_OTG_FS).
 * Beide als Alternate Function AF10 (OTG1_FS), Low-Speed GPIO
 * (entspricht CubeMX: GPIO_SPEED_FREQ_LOW, GPIO_NOPULL).
 *
 * WICHTIG: Die OTG_FS-Peripherie-Clock (AHBENR.OTGFSEN) MUSS aktiviert
 * sein, bevor tusb_init() den DWC2-Kern initialisiert. Ohne Clock liefert
 * das GSNPSID-Register 0 und der Synopsys-ID-Check schlägt fehl. */
void bsp_usb_fs_init(void)
{
    /* 1) OTG_FS-Peripherie-Clock aktivieren.
     * WORKAROUND: Die HAL-Makros __HAL_RCC_USB_OTG_FS_CLK_ENABLE() /
     * __HAL_RCC_USB2_OTG_FS_CLK_ENABLE() setzen fälschlich RCC_AHB1ENR.USB2OTGHSEN
     * (bit 28, OTG_HS) statt RCC_AHB1ENR.USB2OTGFSEN (bit 27, OTG_FS) – ein
     * Bug im vendored HAL (stm32h7xx_hal_rcc.h:1059). Ohne die korrekte
     * Bit-Position liest GSNPSID 0, die FS-PHY wird nicht erkannt und der
     * Controller enumeriert nie. Hier das korrekte Bit direkt setzen. */
    SET_BIT(RCC->AHB1ENR, RCC_AHB1ENR_USB2OTGFSEN);

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef usb = {0};
    usb.Mode       = GPIO_MODE_AF_PP;
    usb.Pull       = GPIO_NOPULL;
    usb.Speed      = GPIO_SPEED_FREQ_LOW;
    usb.Alternate  = BSP_USB_FS_GPIO_AF;

    usb.Pin = BSP_USB_FS_PIN_DM;
    HAL_GPIO_Init(BSP_USB_FS_PORT_DM, &usb);   /* PA11 = USB_DM */

    usb.Pin = BSP_USB_FS_PIN_DP;
    HAL_GPIO_Init(BSP_USB_FS_PORT_DP, &usb);   /* PA12 = USB_DP */
}

/* ---------- USB OTG FS IRQ Handler ----------
 * TinyUSB erwartet diesen Handler für Rhport 0 (OTG_FS). Er leitet das
 * Interrupt auf dcd_int_handler() weiter, der den USB-Stack tickt.
 * Ohne diesen Handler bleibt der USB-Controller stumm (keine Enumeration). */
void OTG_FS_IRQHandler(void)
{
    tusb_int_handler(0, true);
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

/* ---------- printf -> USART3 ---------- */
int _write(int file, char *ptr, int len)
{
    (void)file;
    HAL_UART_Transmit(&huart_dbg, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

/* ---------- assert_param (USE_FULL_ASSERT) ----------
 * Vom HAL via hal_conf.h bei USE_FULL_ASSERT deklariert. Meldet den
 * Auslöser auf die UART (Terminal) und läuft weiter (Debug-Modus:
 * kein Reset, kein Halt – bewusst gewählte Konfiguration). */
void assert_failed(uint8_t *file, uint32_t line)
{
    printf("\r\n[ASSERT FAILED] %.*s, line %lu – HAL-Parameter ungültig (weiterlaufen)\r\n",
           64, (const char *)file, (unsigned long)line);
}
