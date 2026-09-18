#ifndef _I2C_WRAPPER_H_
#define _I2C_WRAPPER_H_

#include <stdint.h>
#include "esp_err.h"

typedef struct sI2c_wrapper_transaction
{
    uint8_t writing;
    uint8_t register_address;
    uint8_t register_length;
    uint8_t *data;
    uint8_t length;
} dtI2c_wrapper_transaction;

void I2C_Wrapper_Init(void);
void I2C_Wrapper_SetDevice(uint8_t SlaveAddress, uint32_t freq);
esp_err_t I2C_Wrapper_Transmit(dtI2c_wrapper_transaction *const transaction);

#endif