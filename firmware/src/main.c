/**
 * RHYTHM – Main
 * STM32H753ZI (NUCLEO-H753ZI als Dev-Board; final STM32H743ZI)
 *
 * Phase 2c: UAC1 USB-Audio-Prototyp (TinyUSB, OTG_FS Rhport 0, interne FS-PHY).
 *   - USB Audio Class 1.0, stereo playback, 48 kHz, 16-bit
 *   - Asynchroner Feedback (FIFO-count)
 *   - Audiodaten werden in einen RAM-Ringbuffer geschrieben (noch kein SAI/DAC).
 *
 * Initialisierung entspricht dem CubeMX-Referenzprojekt:
 *   MPU_Config → HAL_Init → SystemClock_Config (480 MHz VOS0) → GPIO → USB_OTG_FS
 * Danach: TinyUSB UAC1-Init + Main-Loop (tud_task + usb_audio_task).
 *
 * Debug über UART (USART3) – keine printf im Isochronous-ISR-Pfad.
 */
#include "main.h"
#include "bsp.h"
#include "board_clock.h"
#include "periodic.h"
#include "usb_audio.h"
#include "audio_buffer.h"
#include "tusb.h"
#include "stm32h7xx.h"
#include "common_types.h"
#include <stdio.h>

static volatile bool s_usb_configured = false;
static volatile bool s_usb_reset = false;
static volatile bool s_usb_suspended = false;

//--------------------------------------------------------------------
// TinyUSB device callbacks (device level, NOT the audio class)
//--------------------------------------------------------------------
uint32_t tud_get_extra_status_cb(uint8_t type, uint16_t addr) {
  (void)type; (void)addr;
  return 0;
}

//--------------------------------------------------------------------
// Debug: status line via UART (called periodically, NOT in ISR path)
//--------------------------------------------------------------------
static void debug_print_status(void) {
  const audio_buffer_stats_t *st = audio_buffer_stats();
  printf("[UAC1] %s | %u Hz | rx=%u pkt / %u B | overrun=%u underrun=%u | %s%s%s\r\n",
         s_usb_configured ? "configured" : "not configured",
         (unsigned)usb_audio_sample_rate(),
         (unsigned)st->rx_packets,
         (unsigned)st->rx_bytes,
         (unsigned)st->push_overruns,
         (unsigned)st->pop_underruns,
         s_usb_reset ? "RESET" : "",
         s_usb_suspended ? " SUSPENDED" : "",
         usb_audio_streaming() ? " STREAMING" : "");
}

/* ── MPU-Konfiguration (1:1 aus CubeMX) ────────────────────────────────── */
static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /* Initializes and configures the Region and the memory to be protected */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

int main(void)
{
    /* ── CubeMX-Konfiguration (entspricht tools/ST32CubeMX_project_for/Rythm) ── */

    /* 1) MPU vor allem anderen (CubeMX: MPU_Config in main vor HAL_Init). */
    MPU_Config();

    /* 2) HAL-Init (Systick, NVIC-PriorityGroup). */
    HAL_Init();

    /* 3) System-Clock: 480 MHz VOS0, HSI-PLL1, LSE+CRS, HSI48 für USB.
     *    (board_clock_init() ersetzt CubeMX SystemClock_Config.) */
    if (!board_clock_init()) {
        printf("ERROR: board_clock_init() failed (err=%lu), staying on HSI 64 MHz\r\n",
               (unsigned long)g_board_clock_error);
        while (1) { bsp_toggle_led(); bsp_delay_ms(200); }
    }
    printf("Clock: %u MHz (VOS0, HSI-PLL1)\r\n", (unsigned)(board_clock_hz() / 1000000));

    /* 4) GPIO-Init (LED, UART, USB FS-Pins). */
    bsp_init();

    /* 5) USB OTG FS – NVIC + IRQ (CubeMX: OTG_FS_IRQn, Priority 0). */
    HAL_NVIC_SetPriority(OTG_FS_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);

    /* ── RHYTHM-Applikation ──────────────────────────────────────────────── */

    printf("RHYTHM firmware started (UAC1, %u Hz, 2ch, 16-bit)\r\n",
           (unsigned)CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_FS);

    /* USB-Audio-Device (TinyUSB UAC1) initialisieren.
     * tusb_init() bringt den USB-Stack auf Rhport 0 (OTG_FS) hoch. */
    audio_buffer_init();
    tusb_rhport_init_t dev_init = {
        .role  = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_FULL
    };
    if (!tusb_init(0, &dev_init)) {
        printf("ERROR: tusb_init() FAILED\r\n");
        while (1) { bsp_toggle_led(); bsp_delay_ms(200); }
    }
    printf("USB Audio Device initialized (TinyUSB UAC1, OTG_FS Rhport0, FS mode)\r\n");
    printf("Waiting for USB enumeration...\r\n");

    uint32_t last_led_ms    = HAL_GetTick();
    uint32_t last_status_ms = HAL_GetTick();
    bool     warned_no_mount = false;

    while (1)
    {
        /* USB-Stack ticken (ISRs werden hier nur gesteuert, nicht im ISR-Pfad) */
        tud_task();

        /* Audiodaten aus TinyUSB-RX-FIFO in den Ringbuffer pumpen */
        usb_audio_task();

        const uint32_t now = HAL_GetTick();

        /* LED: alle 10 s togglen (Status) */
        if (periodic_due(now, &last_led_ms, 10000U)) {
            bsp_toggle_led();
        }

        /* Debug-Status: alle 2 s auf UART */
        if (periodic_due(now, &last_status_ms, 2000U)) {
            debug_print_status();
        }

        /* WARNUNG: Nach 5 s ohne Enumeration -> Hinweis */
        if (!warned_no_mount && now >= 5000) {
            if (!s_usb_configured) {
                printf("WARN: USB not mounted after 5 s. Check:\r\n");
                printf("  1. USB cable connected (VBUS on PA9?)\r\n");
                printf("  2. OTG_FS clock enabled (RCC->AHBENR.OTGFSEN)\r\n");
                printf("  3. IRQ handler active (OTG_FS_IRQHandler)\r\n");
                printf("  4. PHY type: FS internal (PA11/12, AF10)\r\n");
                warned_no_mount = true;
            }
        }

        bsp_delay_ms(1);
    }

    return 0;
}

//--------------------------------------------------------------------
// USB device callbacks (enumeration / reset / suspend / resume)
//--------------------------------------------------------------------
void tud_mount_cb(void) {
  s_usb_configured = true;
  printf("[USB] mounted (enumeration OK)\r\n");
}

void tud_umount_cb(void) {
  s_usb_configured = false;
  printf("[USB] unmounted\r\n");
}

void tud_suspended_cb(void) {
  s_usb_suspended = true;
  printf("[USB] suspended\r\n");
}

void tud_resume_cb(void) {
  s_usb_suspended = false;
  printf("[USB] resumed\r\n");
}
