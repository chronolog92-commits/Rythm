#ifndef USB_AUDIO_H_
#define USB_AUDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Pump TinyUSB PCM bytes through the ring into the Float32 block queue.
// Call once per millisecond from the main loop (not from the ISR).
void     usb_audio_task(void);
// Configuration/unmount reset; call in task context with consumer quiescent.
void     usb_audio_reset(void);
uint32_t usb_audio_sample_rate(void);
bool     usb_audio_streaming(void);

#ifdef __cplusplus
}
#endif

#endif
