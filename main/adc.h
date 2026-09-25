#ifndef ADC_H_
#define ADC_H_

#include "esp_adc/adc_oneshot.h"

extern void adc_init(void);
extern uint16_t adc_get_volt(adc_channel_t ch);
extern void adc_cont_start(void);
extern void adc_cont_stop(void);
extern uint32_t adc_get_raw(adc_channel_t ch, uint16_t* buff, uint16_t max_len);

#endif