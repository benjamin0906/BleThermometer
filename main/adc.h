#ifndef ADC_H_
#define ADC_H_

#include "esp_adc/adc_oneshot.h"

extern void adc_init(void);
extern uint16_t adc_get_volt(adc_channel_t ch);
extern uint16_t adc_get_raw(adc_channel_t ch);
extern void adc_ConfigConversion(adc_channel_t ch);
extern void adc_TriggerOneshot(void);
extern uint8_t adc_IsOneshotDone(void);
extern uint16_t adc_GetLastRaw(void);
extern uint16_t adc_GetLastVoltage(void);
#endif