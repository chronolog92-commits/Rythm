/*
 * audio_config.h - Central audio configuration for the RHYTHM project.
 *
 * Phase 1: stereo playback, 48 kHz, 24-bit in 32-bit slots.
 * Future phases extend this (input channels, higher rates, SAI).
 */
#ifndef AUDIO_CONFIG_H_
#define AUDIO_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

//--------------------------------------------------------------------
// Phase 1 playback configuration
//--------------------------------------------------------------------
#define AUDIO_NUM_OUTPUT_CHANNELS       2
#define AUDIO_NUM_INPUT_CHANNELS        0   // Phase 5
#define AUDIO_SAMPLE_RATE               48000
#define AUDIO_BITS_PER_SAMPLE           24
#define AUDIO_SLOT_BYTES                4   // 32-bit slots
#define AUDIO_BYTES_PER_SAMPLE          AUDIO_SLOT_BYTES

// Bytes per sample-frame (all channels) = channels * slot bytes.
#define AUDIO_BYTES_PER_FRAME           (AUDIO_NUM_OUTPUT_CHANNELS * AUDIO_SLOT_BYTES)

// Microframe period (HS: 125 us). Samples per microframe at 48 kHz:
//   48000 * 0.000125 = 6 samples -> 6 * 8 = 48 bytes per microframe max.
// The TinyUSB EP size macro computes this automatically.

//--------------------------------------------------------------------
// Debug / diagnostics
//--------------------------------------------------------------------
#define AUDIO_DEBUG_UART_BAUDRATE       115200
#define AUDIO_DEBUG_UART_PORT           USART6
// LED pins for status (enumeration, streaming, errors) - see bsp.h.
#define AUDIO_LED_STATUS_PORT           GPIOB
#define AUDIO_LED_STATUS_PIN            GPIO_PIN_1

#ifdef __cplusplus
}
#endif

#endif
