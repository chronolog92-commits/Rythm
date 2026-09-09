/**
 * RHYTHM – main.h
 * Zentrales HAL-Entry-Point.
 *
 * Alle HAL-/CMSIS-Quellen sind unter vendor_<board>/ im Repo
 * (vendor_h743/ für das aktuelle Dev-Board, vendor_f767/ als Fallback).
 * Der Build (platformio.ini) zeigt die Include-Pfade dorthin; CubeMX ist NICHT
 * erforderlich.
 *
 * Das Header ist board-agnostisch: Es wählt die passenden CMSIS-/HAL-Header je
 * nach aktivem STM32-Teil (STM32H7xx → H743, sonst F767). So kann dieselbe
 * Anwendung (src-Ordner) für beide Boards gebaut werden.
 *
 * Später (wenn der Clock-Tree final steht) kann hier ein CubeMX-Generat
 * eingesetzt werden – dann einfach die Module aus bsp.c/HAL-Init ersetzen.
 */

#ifndef RHYTHM_MAIN_H
#define RHYTHM_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(STM32H7xx)
/* CMSIS Device-Header (stm32h7xx.h, system_stm32h7xx.h) – NUCLEO-H743ZI */
#include "stm32h7xx.h"
/* HAL-Basis + Module (RCC, GPIO, DMA, I2C, I2S, SPI, TIM, UART, PWR, EXTI, FLASH, CORTEX) */
#include "stm32h7xx_hal.h"
#elif defined(STM32F767xx)
/* CMSIS Device-Header (stm32f7xx.h, system_stm32f7xx.h) – NUCLEO-F767ZI (Fallback) */
#include "stm32f7xx.h"
/* HAL-Basis + Module (RCC, GPIO, DMA, I2C, I2S, SPI, TIM, UART, PWR, EXTI, FLASH, CORTEX) */
#include "stm32f7xx_hal.h"
#else
#error "RHYTHM: Kein bekanntes STM32-Teil definiert (STM32H7xx oder STM32F767xx erwartet)."
#endif

#ifdef __cplusplus
}
#endif

#endif /* RHYTHM_MAIN_H */
