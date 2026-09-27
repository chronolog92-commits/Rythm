/**
 * RHYTHM – board_clock.c
 *
 * System-Clock-Initialisierung: 480 MHz (VOS0, HSI-PLL1) für STM32H753ZI.
 * Doku + Begründung: docs/clock-verification.md
 *
 * Konfiguration entspricht dem CubeMX-Referenzprojekt (tools/ST32CubeMX_project_for
 * generation/Rythm/Rythm.ioc): HSI-PLL, VOS0 (Overdrive), LSE + CRS, HSI48 für USB.
 */

#include "board_clock.h"

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_rcc.h"
#include "stm32h7xx_hal_rcc_ex.h"
#include "stm32h7xx_hal_pwr.h"
#include "stm32h7xx_hal_pwr_ex.h"
#include <stdio.h>

uint32_t g_board_clock_error = 0;

/* ── PLL1-Parameter (480 MHz aus HSI 64 MHz) ─────────────────────────────── */
#define RHYTHM_PLL1_M      4U      /* 64 MHz / 4 = 16 MHz (Range 3: 8–16 MHz) */
#define RHYTHM_PLL1_N      60U     /* 16 MHz × 60 = 960 MHz VCO               */
#define RHYTHM_PLL1_P      2U      /* 960 MHz / 2 = 480 MHz SYSCLK            */
#define RHYTHM_PLL1_Q      4U      /* 960/4 = 240 MHz (unbenutzt)             */
#define RHYTHM_PLL1_R      6U      /* 960/6 = 160 MHz (unbenutzt)             */
#define RHYTHM_PLL1_RGE    RCC_PLL1VCIRANGE_3   /* VCOIN 8–16 MHz  */
#define RHYTHM_PLL1_VCOSEL RCC_PLL1VCOWIDE      /* WIDE-Band       */

#define RHYTHM_FLASH_LATENCY FLASH_LATENCY_4 /* für 480 MHz mit Overdrive */

/* Timeout (ms) für PLL1-Startup / SYSCLK-Switch / LSE / HSI48 */
#define RHYTHM_CLK_TIMEOUT_MS 100U

/**
 * @brief  480 MHz (VOS0, HSI-PLL1) einrichten.
 *
 * @retval 1 Erfolg (SYSCLK = 480 MHz), 0 Fehlschlag (bleibt auf HSI 64 MHz).
 */
int board_clock_init(void)
{
    g_board_clock_error = 0;

    /* 1) Versorgung: LDO-Modus (entspricht CubeMX: PWR_LDO_SUPPLY).
     *    ExitRun0Mode() hat bereits PWR_CR3.LDOEN gesetzt; hier sicherstellen. */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    /* 2) Spannungsskalierung auf VOS0 (Overdrive). MUSS vor PLL1-Switch.
     *    VOS0 braucht SYSCFG ODEN + ODSWEN/ODSWRDY (CubeMX macht das in
     *    SystemClock_Config; hier äquivalent). */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    /* Overdrive aktivieren (SYSCFG->CFGR: ODEN + ODSWEN). */
    {
        volatile uint32_t *cfgr = (volatile uint32_t *)0x58024400; /* SYSCFG->CFGR */
        *cfgr |= (1U << 13) | (1U << 12); /* ODSWEN | ODEN */
        /* Auf ODSWRDY (Bit 14) warten. */
        uint32_t t0 = HAL_GetTick();
        while (!(*cfgr & (1U << 14)))
        {
            if (HAL_GetTick() - t0 > RHYTHM_CLK_TIMEOUT_MS)
            {
                g_board_clock_error = 2;
                return 0;
            }
        }
    }

    /* 3) Flash-Latenz für 480 MHz (BEVOR der Takt hochgeschaltet wird). */
    __HAL_FLASH_SET_LATENCY(RHYTHM_FLASH_LATENCY);
    if (__HAL_FLASH_GET_LATENCY() != RHYTHM_FLASH_LATENCY)
    {
        g_board_clock_error = 4;
        return 0;
    }

    /* 4) LSE aktivieren (32,768 kHz, für CRS-Sync + RTC).
     *    NUCLEO-H753ZI hat LSE auf PC14/PC15. */
    {
        RCC_OscInitTypeDef lse_cfg = {0};
        lse_cfg.OscillatorType = RCC_OSCILLATORTYPE_LSE;
        lse_cfg.LSEState       = RCC_LSE_ON;
        if (HAL_RCC_OscConfig(&lse_cfg) != HAL_OK)
        {
            g_board_clock_error = 6;
            return 0; /* LSE nicht verfügbar – nicht fatal, aber CRS geht nicht */
        }
    }

    /* 5) HSI + PLL1 konfigurieren.
     *    WICHTIG: HSEState = RCC_HSE_OFF — das Board hat KEIN HSE.
     *    PLL1 wird über osc.PLL.* + PLL.PLLState = RCC_PLL_ON konfiguriert. */
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType  = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSE;
    osc.HSEState        = RCC_HSE_OFF;
    osc.LSEState        = RCC_LSE_ON;
    osc.HSIState        = RCC_HSI_DIV1;
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

    /* 6) SYSCLK auf PLL1 schalten + Bus-Teiler (entspricht CubeMX). */
    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 |
                         RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.SYSCLKDivider  = RCC_SYSCLK_DIV1;
    clk.AHBCLKDivider  = RCC_HCLK_DIV2;      /* HCLK = 240 MHz */
    clk.APB3CLKDivider = RCC_APB3_DIV2;      /* APB3 = 240 MHz */
    clk.APB1CLKDivider = RCC_APB1_DIV2;      /* APB1 = 240 MHz */
    clk.APB2CLKDivider = RCC_APB2_DIV2;      /* APB2 = 240 MHz */
    clk.APB4CLKDivider = RCC_APB4_DIV2;      /* APB4 = 240 MHz */

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_DEFAULT) != HAL_OK)
    {
        g_board_clock_error = 3;
        return 0;
    }

    /* 7) CRS konfigurieren (LSE als Sync-Quelle für HSI-Trimming).
     *    Entspricht CubeMX: RCC_CRS_SYNC_SOURCE_LSE, DIV1, rising edge. */
    __HAL_RCC_CRS_CLK_ENABLE();
    {
        RCC_CRSInitTypeDef crs = {0};
        crs.Prescaler    = RCC_CRS_SYNC_DIV1;
        crs.Source       = RCC_CRS_SYNC_SOURCE_LSE;
        crs.Polarity     = RCC_CRS_SYNC_POLARITY_RISING;
        crs.ReloadValue  = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 32768);
        crs.ErrorLimitValue = 34;
        crs.HSI48CalibrationValue = 32;
        HAL_RCCEx_CRSConfig(&crs);
    }

    /* 8) USB-Clock-Source: HSI48 (dedizierter 48-MHz-Oszillator).
     *    Entspricht CubeMX: RCC_USBCLKSOURCE_HSI48. */
    __HAL_RCC_HSI48_ENABLE();
    {
        uint32_t t0 = HAL_GetTick();
        while (!(RCC->CR & RCC_CR_HSI48RDY))
        {
            if (HAL_GetTick() - t0 > RHYTHM_CLK_TIMEOUT_MS)
            {
                g_board_clock_error = 5;
                printf("ERROR: HSI48 didn't start!\r\n");
                return 0;
            }
        }
        printf("LOG: HSI48 started.\r\n");
    }
    RCC->D2CCIP2R = (RCC->D2CCIP2R & ~(uint32_t)RCC_D2CCIP2R_USBSEL)
                  | (uint32_t)RCC_USBCLKSOURCE_HSI48;

    /* 9) SystemCoreClock aktualisieren (für HAL_Delay/Tick). */
    SystemCoreClockUpdate();

    return 1;
}

uint32_t board_clock_hz(void)
{
    return SystemCoreClock;
}
