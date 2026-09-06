/**
 * RHYTHM – main.h
 * Zentrales HAL-Entry-Point.
 *
 * Alle HAL-/CMSIS-Quellen sind unter vendor/ im Repo (siehe tools/vendor_import.py).
 * Der Build (platformio.ini) zeigt die Include-Pfade dorthin; CubeMX ist NICHT
 * erforderlich.
 *
 * Später (wenn der Clock-Tree final steht) kann hier ein CubeMX-Generat
 * eingesetzt werden – dann einfach die Module aus bsp.c/HAL-Init ersetzen.
 */

#ifndef RHYTHM_MAIN_H
#define RHYTHM_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* CMSIS Device-Header (stm32f7xx.h, system_stm32f7xx.h) */
#include "stm32f7xx.h"

/* HAL-Basis + Module (RCC, GPIO, DMA, I2C, I2S, SPI, TIM, UART, PWR, EXTI, FLASH, CORTEX) */
#include "stm32f7xx_hal.h"

#ifdef __cplusplus
}
#endif

#endif /* RHYTHM_MAIN_H */
