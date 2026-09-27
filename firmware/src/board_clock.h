/**
 * RHYTHM – board_clock.h
 *
 * System-Clock-Initialisierung für das MB1364-Dev-Board (STM32H753ZI).
 *
 * Ziel-Takt: 480 MHz (VOS0, Overdrive), HSI-basiert.
 *
 * WICHTIG (Board-Realität, s. docs/clock-verification.md):
 *   - KEIN HSE auf dem Board (nur LSE 32,768 kHz). HSE darf NICHT als PLL1-Quelle
 *     konfiguriert werden — sonst Timeout/Hang.
 *   - Einzige praktikable PLL1-Eingangquelle: HSI (64 MHz, intern, ±1 %).
 *   - LSE (32,768 kHz) wird für CRS-Sync und RTC verwendet.
 *
 * PLL1: HSI 64 MHz / M=4 -> 16 MHz (Range 3: 8–16 MHz)
 *       N=60, VCOSEL=WIDE  -> VCO 960 MHz
 *       P=2 -> SYSCLK 480 MHz
 * Busse: HCLK /2 (240), APB3 /2 (240), APB1 /2 (240), APB2 /2 (240), APB4 /2 (240)
 * Flash: 4 Waitstates (für 480 MHz mit Overdrive).
 *
 * Aufruf: board_clock_init() MUSS VOR der Peripherie-Initialisierung laufen
 * (in bsp_init() nach HAL_Init(), aber vor GPIO-Setup).
 *
 * Fehlerpfad: Bei Fehlschlag (PLL1 läuft nicht an) bleibt der Chip auf HSI 64 MHz
 * (sicher) und die Funktion liefert 0 — der Rest des Systems läuft weiter, nur
 * langsamer. Kein Hardfault, kein Hang.
 */

#ifndef RHYTHM_BOARD_CLOCK_H
#define RHYTHM_BOARD_CLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  System-Clock auf 480 MHz (VOS0, HSI-PLL1) setzen.
 *
 * @retval 1  Erfolg: SYSCLK = 480 MHz, SystemCoreClock aktualisiert.
 * @retval 0  Fehlschlag: Chip bleibt auf HSI 64 MHz (sicher). Ursache wird in
 *             g_board_clock_error kodiert (1=PLL1-Startup-Timeout, 2=VOS-Setup,
 *             3=SYSCLK-Switch-Timeout, 4=Flash-Latenz, 5=HSI48-Timeout, 6=LSE-Timeout).
 *
 * @note   Muss nach HAL_Init() und VOR der Peripherie-Init (bsp_gpio_init) laufen.
 */
int board_clock_init(void);

/**
 * @brief  Letztes Fehlercode von board_clock_init() (0 = kein Fehler).
 */
extern uint32_t g_board_clock_error;

/**
 * @brief  Tatsächlicher Systemtakt nach board_clock_init() (Hz).
 *
 * @note   Liest SystemCoreClock (wird von board_clock_init() gesetzt). Vor dem
 *         Aufruf von board_clock_init() ist der Wert 64000000 (HSI-Default).
 */
uint32_t board_clock_hz(void);

#ifdef __cplusplus
}
#endif

#endif /* RHYTHM_BOARD_CLOCK_H */
