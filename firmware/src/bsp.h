/**
 * RHYTHM – Board Support Package (BSP)
 * Zentrale Pin-Map und Low-Level-Initialisierung.
 *
 * Pin-Map ist VORLÄUFIG und muss mit dem NUCLEO-F767ZI-Pinout abgeglichen werden.
 * Quelle: docs/architecture.md (Peripherie-Nutzung).
 */

#ifndef RHYTHM_BSP_H
#define RHYTHM_BSP_H

#include "main.h"

/* ---------- LED ---------- */
#define BSP_LED_PORT      GPIOB
#define BSP_LED_PIN       GPIO_PIN_7    /* NUCLEO-F767ZI: LD2 blau, aktiv high (UM1974). */

/* ---------- TOSLINK ---------- */
/* Eingang: PLR135/T → GPIO → SW-S/PDIF-Decoder (AN5073) */
#define SPDIF_RX_PORT     GPIOD
#define SPDIF_RX_PIN      GPIO_PIN_13
/* Ausgang: GPIO → LED-Treiber → TOSLINK-LED */
#define SPDIF_TX_PORT     GPIOD
#define SPDIF_TX_PIN      GPIO_PIN_14

/* ---------- UART (ESP32-S3) ---------- */
#define ESP_UART          UART2
#define ESP_UART_BAUD     115200
/* TX: PD4 (USART2_TX), RX: PD5 (USART2_RX) */
#define ESP_UART_TX_PORT  GPIOD
#define ESP_UART_TX_PIN   GPIO_PIN_4
#define ESP_UART_RX_PORT  GPIOD
#define ESP_UART_RX_PIN   GPIO_PIN_5

/* ---------- I2C (ADC-Konfiguration) ---------- */
#define ADC_I2C           I2C1
/* SCL: PB6, SDA: PB7 (NUCLEO-Standard) */

/* ---------- I2S (Audio zu/von DAC/ADC, ESP32) ---------- */
/* I2S2: BCLK PD12, LRCLK PD13, DOUT PD15, DIN PC1
 * Vorläufig – mit NUCLEO-Pinout abgleichen. */

/* ---------- Encoder (Timer-Input-Capture) ---------- */
/* EC11, A/B + Push. Vorläufige Timer-Zuordnung (siehe architecture.md):
 *   EQ 60Hz  → Timer2   EQ 150Hz → Timer3   EQ 400Hz → Timer4
 *   EQ 1kHz  → Timer5   EQ 2.5k  → Timer6   EQ 6kHz  → Timer7
 *   EQ 12kHz → TIM8_CH1 Volume   → Timer9 */

/* ---------- API ---------- */
void bsp_init(void);
void bsp_toggle_led(void);
void bsp_delay_ms(uint32_t ms);

#endif /* RHYTHM_BSP_H */
