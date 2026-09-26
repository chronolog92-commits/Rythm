/*
 * tusb_config.h - TinyUSB configuration for the RHYTHM UAC2 prototype.
 *
 * Target: STM32H753ZI, USB OTG_FS (Synopsys DWC2, Rhport 0) in FULL-SPEED
 * mode via the internal FS PHY (PA11=DM, PA12=DP). No external ULPI PHY on
 * the dev board - high speed (480 Mbps) requires a USB3300 and comes later.
 * USB Audio Class 2.0, stereo playback, 48 kHz, 24-bit in 32-bit slots,
 * asynchronous (FIFO-count feedback).
 *
 * Port mapping on STM32H753 (see dwc2_stm32.h):
 *   - Rhport 0 -> OTG_FS -> USB2_OTG_FS @ 0x40080000 (FS via internal PHY) <-- used
 *   - Rhport 1 -> OTG_HS -> USB1_OTG_HS @ 0x40040000 (NOT used here)
 */
#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_desc.h"

//--------------------------------------------------------------------
// BOARD CONFIGURATION
//--------------------------------------------------------------------
// USB controller: OTG_FS (Rhport 0) in FULL-SPEED mode (internal FS PHY,
// PA11/PA12). No ULPI on the dev board, so HIGH_SPEED is not available.
// Phase 1 runs at 12 Mbps (Full Speed) - sufficient for 48 kHz/2ch/24-bit.
#ifndef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE   (OPT_MODE_FULL_SPEED | OPT_MODE_DEVICE)
#endif
#ifndef CFG_TUSB_RHPORT1_MODE
#define CFG_TUSB_RHPORT1_MODE   0                       // OTG_HS not used
#endif

// RHPort max operational speed can defined by board.mk
#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED   OPT_MODE_DEFAULT_SPEED
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------
#if defined(CFG_TUSB_RHPORT0_MODE) || defined(CFG_TUSB_RHPORT1_MODE)
  #define CFG_TUSB_MCU          OPT_MCU_STM32H7
#else
  #error "CFG_TUSB_MCU is not defined"
#endif

#define CFG_TUSB_OS             OPT_OS_NONE

// CFG_TUSB_DEBUG: 0=no, 1=ASSERT, 2=print via CFG_TUD_DEBUG_LOG
#define CFG_TUSB_DEBUG          1

// Enable Device stack
#define CFG_TUD_ENABLED       1

// Default is max speed that hardware controller could support with on-chip PHY
#define CFG_TUD_MAX_SPEED     BOARD_TUD_MAX_SPEED

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN        __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

// Expose audio class debug information via HID interface (Phase 1: disabled)
#ifndef CFG_AUDIO_DEBUG
#define CFG_AUDIO_DEBUG           0
#endif

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE  64
#endif

//------------- CLASS -------------//
#define CFG_TUD_AUDIO             1
#define CFG_TUD_HID               0

#define CFG_TUD_CDC               0
#define CFG_TUD_MSC               0
#define CFG_TUD_MIDI              0
#define CFG_TUD_VENDOR            0

//--------------------------------------------------------------------
// MEMORY / CACHE (D-Cache for USB DMA on STM32H7)
//--------------------------------------------------------------------
// USB OTG FS DMA on STM32H7 requires D-Cache maintenance (clean/invalidate)
// for buffer coherency. Enable D-Cache with 32-byte line size.
#define CFG_TUD_MEM_DCACHE_ENABLE    1
#define CFG_TUD_MEM_DCACHE_LINE_SIZE 32

//--------------------------------------------------------------------
// AUDIO CLASS DRIVER CONFIGURATION
//--------------------------------------------------------------------

#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX              2

// 16bit data in 16bit slots
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX      2
#define CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX              16

// UAC1 Full-Speed endpoint size
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_FS     48000
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_FS           TUD_AUDIO_EP_SIZE(false, CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_FS, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX)
// UAC2 High-Speed endpoint size
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_HS     96000
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_HS           TUD_AUDIO_EP_SIZE(true, CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_HS, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX)

#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX          TU_MAX(CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_FS, CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_HS)

// AUDIO_FEEDBACK_METHOD_FIFO_COUNT needs buffer size >= 4* EP size to work correctly
// Example read FIFO every 1ms (8 HS frames), so buffer size should be 8 times larger for HS device
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ       TU_MAX(4 * CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_FS, 32 * CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_HS)

// Enable OUT EP
#define CFG_TUD_AUDIO_ENABLE_EP_OUT                 1

// Enable feedback EP
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP            1

//--------------------------------------------------------------------
// APPLICATION CONFIGURATION (RHYTHM)
//--------------------------------------------------------------------
// Number of audio functions (1) and interface numbering.
//   itf 0 = Audio Control, itf 1 = Audio Streaming (alt 0/1).
#define CFG_TUD_AUDIO_N_FUNCS         1

// UAC2 entity IDs (must match usbd_desc.c).
#define UAC2_ENTITY_INPUT_TERMINAL    0x01
#define UAC2_ENTITY_FEATURE_UNIT      0x02
#define UAC2_ENTITY_OUTPUT_TERMINAL   0x03
#define UAC2_ENTITY_CLOCK             0x04

// Supported sample rates (UAC2 clock entity). Phase 1: 48 kHz only.
#define UAC2_N_SAMPLE_RATES           1

// Function 1 descriptor length (must match usbd_desc.c).
// IAD(8) + AC(9) + CS-AC(9) + ClkSrc(8) + InTerm(17) + OutTerm(12)
// + FeatureUnit 3ch (6 + 4*3 = 18) + AS alt0(9) + AS alt1(9) + CS-AS(16)
// + TypeI(6) + ISO-EP(7) + CS-ISO-EP(8) + ISO-FB-EP(7) = 145
#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN 145

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H_ */
