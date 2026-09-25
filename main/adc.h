#ifndef ADC_H_
#define ADC_H_

#include "esp_adc/adc_oneshot.h"

extern void adc_init(void);
extern uint16_t adc_get_volt(adc_channel_t ch);
extern uint16_t adc_get_raw(adc_channel_t ch);

#endif