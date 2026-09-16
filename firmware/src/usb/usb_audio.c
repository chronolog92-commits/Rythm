/*
 * usb_audio.c - UAC2 application callbacks for the RHYTHM prototype.
 *
 * Implements the TinyUSB audio device callbacks:
 *   - tud_audio_set_itf_cb: track streaming state (alt 0/1)
 *   - tud_audio_set_req_entity_cb / tud_audio_get_req_entity_cb:
 *       UAC2 Clock Source (freq cur/range, valid) and Feature Unit (mute/vol)
 *   - tud_audio_feedback_params_cb: FIFO-count feedback method
 *   - tud_audio_rx_done_isr: called in ISR context when a packet arrives
 *
 * Data path (Phase 1): USB ISO OUT -> TinyUSB internal FIFO -> (audio_task)
 * -> audio_buffer (application ring buffer). No SAI/DAC yet (Phase 3).
 */
#include "tusb.h"
#include "usb_audio.h"
#include "audio_buffer.h"

//--------------------------------------------------------------------
// State
//--------------------------------------------------------------------
static uint32_t  s_sample_rate   = 48000;
static bool      s_streaming     = false;
static uint8_t   s_alt_setting   = 0;

// UAC2 supported sample rates (Phase 1: 48 kHz only).
static const uint32_t s_sample_rates[UAC2_N_SAMPLE_RATES] = { 48000 };

//--------------------------------------------------------------------
// UAC2 entity: Clock Source
//--------------------------------------------------------------------
static bool audio20_clock_get(uint8_t rhport, tusb_control_request_t const *p_request) {
  uint8_t const ctrl_sel = TU_U16_HIGH(p_request->wValue);

  if (ctrl_sel == AUDIO20_CS_CTRL_SAM_FREQ) {
    if (p_request->bRequest == AUDIO20_CS_REQ_CUR) {
      audio20_control_cur_4_t curf = {(int32_t) tu_htole32(s_sample_rate)};
      return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &curf, sizeof(curf));
    } else if (p_request->bRequest == AUDIO20_CS_REQ_RANGE) {
      audio20_control_range_4_n_t(UAC2_N_SAMPLE_RATES) rangef = {
          .wNumSubRanges = tu_htole16(UAC2_N_SAMPLE_RATES)};
      for (uint8_t i = 0; i < UAC2_N_SAMPLE_RATES; i++) {
        rangef.subrange[i].bMin = (int32_t) s_sample_rates[i];
        rangef.subrange[i].bMax = (int32_t) s_sample_rates[i];
        rangef.subrange[i].bRes = 0;
      }
      return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &rangef, sizeof(rangef));
    }
  } else if (ctrl_sel == AUDIO20_CS_CTRL_CLK_VALID &&
             p_request->bRequest == AUDIO20_CS_REQ_CUR) {
    audio20_control_cur_1_t cur_valid = {.bCur = 1};
    return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &cur_valid, sizeof(cur_valid));
  }
  return false;
}

static bool audio20_clock_set(tusb_control_request_t const *p_request, uint8_t const *buf) {
  uint8_t const ctrl_sel = TU_U16_HIGH(p_request->wValue);
  TU_VERIFY(p_request->bRequest == AUDIO20_CS_REQ_CUR);

  if (ctrl_sel == AUDIO20_CS_CTRL_SAM_FREQ) {
    TU_VERIFY(p_request->wLength == sizeof(audio20_control_cur_4_t));
    s_sample_rate = (uint32_t) ((audio20_control_cur_4_t const *) buf)->bCur;
    return true;
  }
  return false;
}

//--------------------------------------------------------------------
// UAC2 entity: Feature Unit (mute / volume)
//--------------------------------------------------------------------
static uint8_t s_mute[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1];
static int16_t s_volume[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1];

static bool audio20_feature_unit_get(uint8_t rhport, tusb_control_request_t const *p_request) {
  uint8_t const ctrl_sel    = TU_U16_HIGH(p_request->wValue);
  uint8_t const channel_num = TU_U16_LOW(p_request->wValue);

  if (ctrl_sel == AUDIO20_FU_CTRL_MUTE && p_request->bRequest == AUDIO20_CS_REQ_CUR) {
    audio20_control_cur_1_t mute1 = {.bCur = s_mute[channel_num]};
    return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &mute1, sizeof(mute1));
  } else if (ctrl_sel == AUDIO20_FU_CTRL_VOLUME) {
    if (p_request->bRequest == AUDIO20_CS_REQ_CUR) {
      audio20_control_cur_2_t cur_vol = {.bCur = tu_htole16(s_volume[channel_num])};
      return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &cur_vol, sizeof(cur_vol));
    } else if (p_request->bRequest == AUDIO20_CS_REQ_RANGE) {
      audio20_control_range_2_n_t(1) range_vol = {
          .wNumSubRanges = tu_htole16(1),
          .subrange[0] = {.bMin = tu_htole16(-50 * 256), tu_htole16(0), tu_htole16(256)}};
      return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &range_vol, sizeof(range_vol));
    }
  }
  return false;
}

static bool audio20_feature_unit_set(tusb_control_request_t const *p_request, uint8_t const *buf) {
  uint8_t const ctrl_sel    = TU_U16_HIGH(p_request->wValue);
  uint8_t const channel_num = TU_U16_LOW(p_request->wValue);
  TU_VERIFY(p_request->bRequest == AUDIO20_CS_REQ_CUR);

  if (ctrl_sel == AUDIO20_FU_CTRL_MUTE) {
    TU_VERIFY(p_request->wLength == sizeof(audio20_control_cur_1_t));
    s_mute[channel_num] = ((audio20_control_cur_1_t const *) buf)->bCur;
    return true;
  } else if (ctrl_sel == AUDIO20_FU_CTRL_VOLUME) {
    TU_VERIFY(p_request->wLength == sizeof(audio20_control_cur_2_t));
    s_volume[channel_num] = (int16_t) ((audio20_control_cur_2_t const *) buf)->bCur;
    return true;
  }
  return false;
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

bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *buf) {
  (void) rhport;
  uint8_t const entity_id = TU_U16_HIGH(p_request->wIndex);
  if (entity_id == UAC2_ENTITY_CLOCK) {
    return audio20_clock_set(p_request, buf);
  }
  if (entity_id == UAC2_ENTITY_FEATURE_UNIT) {
    return audio20_feature_unit_set(p_request, buf);
  }
  return false;
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  uint8_t const entity_id = TU_U16_HIGH(p_request->wIndex);
  if (entity_id == UAC2_ENTITY_CLOCK) {
    return audio20_clock_get(rhport, p_request);
  }
  if (entity_id == UAC2_ENTITY_FEATURE_UNIT) {
    return audio20_feature_unit_get(rhport, p_request);
  }
  return false;
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
