/**
 * RHYTHM – Board Support Package (BSP)
 * Zentrale Pin-Map und Low-Level-Initialisierung.
 *
 * Phase 1: USB OTG_FS (interne FS-PHY, PA11/PA12) – entspricht CubeMX-Konfiguration.
 * High Speed (480 Mbps) kommt später mit externem PHY.
 */
#ifndef RHYTHM_BSP_H_
#define RHYTHM_BSP_H_

#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- LEDs -----------------------------------------------------------
// NUCLEO-H753ZI / H743ZI:
//   - PB7: blaue LED LD2 (User-LED, aktiv-high).
#define BSP_LED_PORT     GPIOB
#define BSP_LED_PIN      GPIO_PIN_7

// --- USB OTG_FS – Full Speed (interne FS-PHY) -----------------------
// PA11 = USB_DM, PA12 = USB_DP (USB_OTG_FS, AF10).
#define BSP_USB_FS_PORT_DM    GPIOA
#define BSP_USB_FS_PIN_DM     GPIO_PIN_11
#define BSP_USB_FS_PORT_DP    GPIOA
#define BSP_USB_FS_PIN_DP     GPIO_PIN_12
#define BSP_USB_FS_GPIO_AF    GPIO_AF10_OTG1_FS

// --- Initialisierung ------------------------------------------------
void bsp_init(void);

// USB OTG_FS – Full-Speed GPIO-Setup (PA11/PA12 als AF10).
void bsp_usb_fs_init(void);

// LEDs
void bsp_toggle_led(void);

// Blocking Delay (ms), SysTick-basiert.
void bsp_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* RHYTHM_BSP_H_ */
