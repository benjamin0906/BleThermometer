#ifndef _SHT41_H_
#define _SHT41_H_

#include "esp_err.h"

esp_err_t SHT41_Measure(int16_t *const Temperature, uint16_t *const Humidity);

#endif /* _SHT41_H_ */