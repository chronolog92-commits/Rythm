/*
 * usb_audio.c - UAC1 application callbacks for the RHYTHM prototype.
 *
 * Implements the TinyUSB audio device callbacks:
 *   - tud_audio_set_itf_cb: track streaming state (alt 0/1)
 *   - tud_audio_set_itf_close_ep_cb: track streaming stop
 *   - tud_audio_feedback_params_cb: FIFO-count feedback method
 *
 * UAC1: Feature-unit mute/volume controls and endpoint sample-rate control.
 * Control values are retained for the future SAI/DAC output stage.
 *
 * Data path (Phase 1): USB ISO OUT -> TinyUSB internal FIFO -> (audio_task)
 * -> audio_buffer (application ring buffer). No SAI/DAC yet (Phase 3).
 */
#include "tusb.h"
#include "usb_audio.h"
#include "audio_buffer.h"
#include "common_types.h"

//--------------------------------------------------------------------
// State
//--------------------------------------------------------------------
static uint32_t  s_sample_rate   = 48000;
static bool      s_streaming     = false;
static uint8_t   s_alt_setting   = 0;
static uint8_t   s_mute[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1];
static int16_t   s_volume[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1];

enum {
  VOLUME_MIN = -100 * 256,  /* UAC1 volume uses signed 1/256 dB units. */
  VOLUME_MAX = 0,
  VOLUME_RES = 256,
};

static bool valid_feature_request(tusb_control_request_t const *request) {
  return tu_u16_low(request->wIndex) == ITF_NUM_AUDIO_CONTROL &&
         tu_u16_high(request->wIndex) == UAC1_ENTITY_FEATURE_UNIT &&
         tu_u16_low(request->wValue) <= CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX;
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *request) {
  if (!valid_feature_request(request)) return false;
  uint8_t const channel = tu_u16_low(request->wValue);
  uint8_t const control = tu_u16_high(request->wValue);

  if (control == AUDIO10_FU_CTRL_MUTE && request->bRequest == AUDIO10_CS_REQ_GET_CUR &&
      request->wLength == 1) {
    return tud_audio_buffer_and_schedule_control_xfer(rhport, request, &s_mute[channel], 1);
  }
  if (control == AUDIO10_FU_CTRL_VOLUME && request->wLength == 2) {
    int16_t value;
    switch (request->bRequest) {
      case AUDIO10_CS_REQ_GET_CUR: value = s_volume[channel]; break;
      case AUDIO10_CS_REQ_GET_MIN: value = VOLUME_MIN; break;
      case AUDIO10_CS_REQ_GET_MAX: value = VOLUME_MAX; break;
      case AUDIO10_CS_REQ_GET_RES: value = VOLUME_RES; break;
      default: return false;
    }
    uint8_t data[2] = {(uint8_t)value, (uint8_t)((uint16_t)value >> 8)};
    return tud_audio_buffer_and_schedule_control_xfer(rhport, request, data, sizeof(data));
  }
  return false;
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *request, uint8_t *data) {
  (void)rhport;
  if (!valid_feature_request(request) || request->bRequest != AUDIO10_CS_REQ_SET_CUR) return false;
  uint8_t const channel = tu_u16_low(request->wValue);
  uint8_t const control = tu_u16_high(request->wValue);
  if (control == AUDIO10_FU_CTRL_MUTE && request->wLength == 1 && data[0] <= 1) {
    s_mute[channel] = data[0];
    return true;
  }
  if (control == AUDIO10_FU_CTRL_VOLUME && request->wLength == 2) {
    int16_t const value = (int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
    if (value < VOLUME_MIN || value > VOLUME_MAX || value % VOLUME_RES != 0) return false;
    s_volume[channel] = value;
    return true;
  }
  return false;
}

static bool valid_rate_request(tusb_control_request_t const *request) {
  return request->wIndex == USB_AUDIO_OUT_EP &&
         request->wValue == (AUDIO10_EP_CTRL_SAMPLING_FREQ << 8) && request->wLength == 3;
}

bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const *request) {
  if (!valid_rate_request(request) || request->bRequest != AUDIO10_CS_REQ_GET_CUR) return false;
  uint8_t data[3] = {(uint8_t)s_sample_rate, (uint8_t)(s_sample_rate >> 8), (uint8_t)(s_sample_rate >> 16)};
  return tud_audio_buffer_and_schedule_control_xfer(rhport, request, data, sizeof(data));
}

bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const *request, uint8_t *data) {
  (void)rhport;
  if (!valid_rate_request(request) || request->bRequest != AUDIO10_CS_REQ_SET_CUR) return false;
  uint32_t const rate = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16);
  if (rate != 44100 && rate != 48000) return false;
  s_sample_rate = rate;
  return true;
}

//--------------------------------------------------------------------
// TinyUSB audio callbacks
//--------------------------------------------------------------------
bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  (void) rhport;
  uint8_t const itf = tu_u16_low(p_request->wIndex);
  uint8_t const alt = tu_u16_low(p_request->wValue);

  s_alt_setting = alt;
  if (itf == 1 && alt != 0) {
    s_streaming = true;
  }
  return true;
}

bool tud_audio_set_itf_close_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  (void) rhport;
  uint8_t const itf = tu_u16_low(p_request->wIndex);
  uint8_t const alt = tu_u16_low(p_request->wValue);
  if (itf == 1 && alt == 0) {
    s_streaming = false;
  }
  return true;
}

// Feedback: FIFO-count method. The driver regulates the FIFO fill level to
// half and derives the feedback value from the sample rate. No SOF ISR needed.
void tud_audio_feedback_params_cb(uint8_t func_id, uint8_t alt_itf, audio_feedback_params_t *feedback_param) {
  (void) func_id;
  (void) alt_itf;
  feedback_param->method    = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
  feedback_param->sample_freq = s_sample_rate;
}

//--------------------------------------------------------------------
// Audio task: pump data out of the TinyUSB RX FIFO into the ring buffer.
// Called once per ms from the main loop (NOT from the isochronous ISR).
// Reads the number of packets available and drains them into audio_buffer.
//--------------------------------------------------------------------
static uint8_t  s_rx_buf[CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX];

void usb_audio_task(void) {
  if (!s_streaming) {
    return;
  }

  // Drain all currently-available packets from the TinyUSB FIFO.
  while (tud_audio_n_available(0) > 0) {
    uint16_t got = tud_audio_n_read(0, s_rx_buf, sizeof(s_rx_buf));
    if (got > 0) {
      audio_buffer_push(s_rx_buf, got);
    }
  }
}

uint32_t usb_audio_sample_rate(void) {
  return s_sample_rate;
}

bool usb_audio_streaming(void) {
  return s_streaming;
}
