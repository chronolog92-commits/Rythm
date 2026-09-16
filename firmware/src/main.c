/**
 * RHYTHM – Main
 * STM32H753ZI (NUCLEO-H753ZI als Dev-Board; final STM32H743ZI)
 *
 * Phase 2c: UAC2 USB-Audio-Prototyp (TinyUSB, OTG_FS Rhport 0, interne FS-PHY).
 *   - USB Audio Class 2.0, stereo playback, 48 kHz, 24-bit in 32-bit slots
 *   - Asynchroner Feedback (FIFO-count)
 *   - Audiodaten werden in einen RAM-Ringbuffer geschrieben (noch kein SAI/DAC).
 *
 * Initialisierung entspricht dem CubeMX-Referenzprojekt:
 *   MPU_Config → HAL_Init → SystemClock_Config (480 MHz VOS0) → GPIO → USB_OTG_FS
 * Danach: TinyUSB UAC2-Init + Main-Loop (tud_task + usb_audio_task).
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

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTOTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 25 ms   : streaming data
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum {
  BLINK_STREAMING = 25,
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

enum {
  VOLUME_CTRL_0_DB = 0,
  VOLUME_CTRL_10_DB = 2560,
  VOLUME_CTRL_20_DB = 5120,
  VOLUME_CTRL_30_DB = 7680,
  VOLUME_CTRL_40_DB = 10240,
  VOLUME_CTRL_50_DB = 12800,
  VOLUME_CTRL_60_DB = 15360,
  VOLUME_CTRL_70_DB = 17920,
  VOLUME_CTRL_80_DB = 20480,
  VOLUME_CTRL_90_DB = 23040,
  VOLUME_CTRL_100_DB = 25600,
  VOLUME_CTRL_SILENCE = 0x8000,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

// Audio controls
// Current states
uint8_t mute[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1];   // +1 for master channel 0
int16_t volume[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1];// +1 for master channel 0
uint32_t current_sample_rate = 44100;

// Buffer for speaker data
uint16_t i2s_dummy_buffer[CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ / 2];

void led_blinking_task(void);
void audio_task(void);

#if CFG_AUDIO_DEBUG
void audio_debug_task(void);
uint8_t current_alt_settings;
volatile uint16_t fifo_count;
volatile uint32_t fifo_count_avg;
#endif

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
  printf("[UAC2] %s | %u Hz | rx=%u pkt / %u B | overrun=%u underrun=%u | %s%s%s\r\n",
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

    printf("RHYTHM firmware started (UAC2, %u Hz, 2ch, 24-bit)\r\n",
           (unsigned)CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_FS);

    /* USB-Audio-Device (TinyUSB UAC2) initialisieren.
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
    printf("USB Audio Device initialized (TinyUSB UAC2, OTG_FS Rhport0, FS mode)\r\n");
    printf("Waiting for USB enumeration...\r\n");

    /* DEBUG: USB-Controller-Register-Diagnose (OTG_FS @ 0x40080000) */
    {
        volatile uint32_t* gsnpsid = (volatile uint32_t*)0x40080004;
        volatile uint32_t* ghwcfg2 = (volatile uint32_t*)0x40080014;
        volatile uint32_t* dcfg    = (volatile uint32_t*)0x40080400;
        volatile uint32_t* gintsts = (volatile uint32_t*)0x40080418;

        uint32_t gsnpsid_val = *gsnpsid;
        uint32_t ghwcfg2_val = *ghwcfg2;
        uint32_t dcfg_val    = *dcfg;
        uint32_t gintsts_val = *gintsts;

        printf("=== USB OTG_FS Controller Register Dump ===\r\n");
        printf("GSNPSID:   0x%08lX (erwartet: 0x4F544737)\r\n", (unsigned long)gsnpsid_val);
        printf("GHWCFG2:   0x%08lX (hs_phy=%lu, fs_phy=%lu)\r\n",
               (unsigned long)ghwcfg2_val, (unsigned long)((ghwcfg2_val >> 18) & 0x3),
               (unsigned long)((ghwcfg2_val >> 20) & 0x3));
        printf("DCFG:      0x%08lX\r\n", (unsigned long)dcfg_val);
        printf("GINTSTS:   0x%08lX\r\n", (unsigned long)gintsts_val);
        printf("AHB1ENR:   0x%08lX (OTGFSEN=bit27)\r\n", (unsigned long)RCC->AHB1ENR);
        printf("===========================================\r\n");

        if (gsnpsid_val != 0x4F544737) {
            printf("WARN: GSNPSID unexpected! OTG_FS clock may be off.\r\n");
        }
        if (((ghwcfg2_val >> 20) & 0x3) == 0) {
            printf("WARN: No FS PHY detected! Check PA11/PA12 wiring.\r\n");
        }
    }

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

#if CFG_AUDIO_DEBUG
//--------------------------------------------------------------------+
// HID interface for audio debug
//--------------------------------------------------------------------+
// Every 1ms, we will sent 1 debug information report
void audio_debug_task(void) {
  static uint32_t start_ms = 0;
  uint32_t curr_ms = tusb_time_millis_api();
  if (start_ms == curr_ms) return;// not enough time
  start_ms = curr_ms;

  audio_debug_info_t debug_info;
  debug_info.sample_rate = current_sample_rate;
  debug_info.alt_settings = current_alt_settings;
  debug_info.fifo_size = CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ;
  debug_info.fifo_count = fifo_count;
  debug_info.fifo_count_avg = (uint16_t) (fifo_count_avg >> 16);
  for (int i = 0; i < CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1; i++) {
    debug_info.mute[i] = mute[i];
    debug_info.volume[i] = volume[i];
  }

  if (tud_hid_ready())
    tud_hid_report(0, &debug_info, sizeof(debug_info));
}

// Invoked when received GET_REPORT control request
// Unused here
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
  // TODO not Implemented
  (void) itf;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// Unused here
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
  // This example doesn't use multiple report and report ID
  (void) itf;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) bufsize;
}

#endif