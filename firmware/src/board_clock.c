/**
 * RHYTHM – board_clock.c
 *
 * System-Clock-Initialisierung: 400 MHz (VOS1, HSI-PLL1) für STM32H753ZI.
 * Doku + Begründung: docs/clock-verification.md
 */

#include "board_clock.h"

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_rcc.h"
#include "stm32h7xx_hal_rcc_ex.h"
#include "stm32h7xx_hal_pwr.h"
#include "stm32h7xx_hal_pwr_ex.h"

uint32_t g_board_clock_error = 0;

/* ── PLL1-Parameter (400 MHz aus HSI 64 MHz) ─────────────────────────────── */
#define RHYTHM_PLL1_M      4U      /* 64 MHz / 4 = 16 MHz (Range 3: 8–16 MHz) */
#define RHYTHM_PLL1_N      100U    /* 16 MHz × 100 = 1600 MHz VCO             */
#define RHYTHM_PLL1_P      2U      /* 1600 MHz / 2 = 800… P=2 -> 400 MHz SYSCLK */
#define RHYTHM_PLL1_Q      4U      /* 1600/4 = 400 MHz (USB/periph, ungenutzt) */
#define RHYTHM_PLL1_R      2U      /* 1600/2 = 800 MHz (unbenutzt)            */
#define RHYTHM_PLL1_RGE    RCC_PLL1VCIRANGE_3   /* VCOIN 8–16 MHz  */
#define RHYTHM_PLL1_VCOSEL RCC_PLL1VCOWIDE      /* WIDE-Band       */

#define RHYTHM_FLASH_LATENCY FLASH_ACR_LATENCY_7WS /* konservativ für 400 MHz */

/* Timeout (ms) für PLL1-Startup / SYSCLK-Switch */
#define RHYTHM_CLK_TIMEOUT_MS 100U

/**
 * @brief  400 MHz (VOS1, HSI-PLL1) einrichten.
 *
 * @retval 1 Erfolg (SYSCLK = 400 MHz), 0 Fehlschlag (bleibt auf HSI 64 MHz).
 */
int board_clock_init(void)
{
    g_board_clock_error = 0;

    /* 1) Spannungsskalierung auf VOS1 (kein Overdrive). Muss VOR PLL1-Switch
     *    gesetzt werden. Auf VOS1 (nicht VOS0) ist kein SYSCFG_ODEN nötig. */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
    {
        g_board_clock_error = 2;
        return 0; /* bleibt sicher auf HSI 64 MHz */
    }

    /* 2) Flash-Latenz für 400 MHz (BEVOR der Takt hochgeschaltet wird). */
    __HAL_FLASH_SET_LATENCY(RHYTHM_FLASH_LATENCY);
    if (__HAL_FLASH_GET_LATENCY() != RHYTHM_FLASH_LATENCY)
    {
        g_board_clock_error = 4;
        return 0;
    }

    /* 3) HSI + PLL1 konfigurieren.
     *    WICHTIG: HSEState = RCC_HSE_OFF — das Board hat KEIN HSE. HSE darf hier
     *    nicht ON/BYPASS sein, sonst Timeout. HSI ist die einzige PLL1-Quelle.
     *    PLL1 wird über osc.PLL.* + PLL.PLLState = RCC_PLL_ON konfiguriert
     *    (es gibt kein RCC_OSCILLATORTYPE_PLL1). */
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType  = RCC_OSCILLATORTYPE_HSI;
    osc.HSEState        = RCC_HSE_OFF;   /* kein HSE auf dem Board! */
    osc.LSEState        = RCC_LSE_OFF;
    osc.HSIState        = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.LSIState        = RCC_LSI_OFF;
    osc.HSI48State      = RCC_HSI48_OFF;
    osc.CSIState        = RCC_CSI_OFF;

    osc.PLL.PLLState  = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM      = RHYTHM_PLL1_M;
    osc.PLL.PLLN      = RHYTHM_PLL1_N;
    osc.PLL.PLLP      = RHYTHM_PLL1_P;
    osc.PLL.PLLQ      = RHYTHM_PLL1_Q;
    osc.PLL.PLLR      = RHYTHM_PLL1_R;
    osc.PLL.PLLRGE    = RHYTHM_PLL1_RGE;
    osc.PLL.PLLVCOSEL = RHYTHM_PLL1_VCOSEL;
    osc.PLL.PLLFRACN  = 0;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        g_board_clock_error = 1;
        return 0; /* PLL1 lief nicht an -> bleibt auf HSI */
    }

    /* 4) SYSCLK auf PLL1 schalten + Bus-Teiler. */
    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 |
                         RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.SYSCLKDivider  = RCC_SYSCLK_DIV1;
    clk.AHBCLKDivider  = RCC_HCLK_DIV1;      /* HCLK = 400 MHz */
    clk.APB3CLKDivider = RCC_APB3_DIV1;      /* APB3 = 400 MHz */
    clk.APB1CLKDivider = RCC_APB1_DIV2;      /* APB1 = 200 MHz */
    clk.APB2CLKDivider = RCC_APB2_DIV1;      /* APB2 = 400 MHz */
    clk.APB4CLKDivider = RCC_APB4_DIV2;      /* APB4 = 200 MHz */

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_DEFAULT) != HAL_OK)
    {
        g_board_clock_error = 3;
        return 0;
    }

    /* 5) SystemCoreClock auf 400 MHz aktualisieren (für HAL_Delay/Tick). */
    SystemCoreClockUpdate();

    return 1;
}

uint32_t board_clock_hz(void)
{
    return SystemCoreClock;
}
