#include <stdint.h>
#include "i2c_wrapper.h"
#include "freertos/FreeRTOS.h"

static const uint8_t SHT41_Cmd_HighWoHeat = 0xFD;
//static const uint8_t SHT41_Cmd_MediWoHeat = 0xF6;
//static const uint8_t SHT41_Cmd_LowWoHeat  = 0xE0;
static const uint16_t DelayTickValue = 10 / portTICK_PERIOD_MS;

static int16_t TemperatureX100;
static uint16_t HumidityX100;

esp_err_t SHT41_Measure(int16_t *const Temperature, uint16_t *const Humidity)
{
    uint16_t tHum = 0xFFFF;
    int16_t TemperatureRaw;
    uint16_t HumidityRaw;
    uint8_t raw_data[6];
    dtI2c_wrapper_transaction transaction1 = {.register_address = SHT41_Cmd_HighWoHeat, .register_length = 1, .writing = 1, .data = raw_data, .length = 0};

    if(I2C_Wrapper_Transmit(&transaction1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    transaction1.writing = 0;
    transaction1.register_length = 0;
    transaction1.length = 6;
    vTaskDelay(DelayTickValue);

    if(I2C_Wrapper_Transmit(&transaction1) != ESP_OK)
    {
        return ESP_FAIL;
    }

    TemperatureRaw = ((uint16_t) raw_data[0] << 8) | ((uint16_t) raw_data[1]);
    TemperatureX100 = (uint16_t)((uint32_t)((uint32_t) TemperatureRaw * 175 * 100 - 294907500)/65535);

    HumidityRaw = ((uint16_t) raw_data[3] << 8) | ((uint16_t) raw_data[4]);
    tHum = (uint16_t)((uint32_t)((uint32_t) HumidityRaw * 125 * 100 - 39321000)/65535);

    if(tHum <= 10000)
    {
        HumidityX100 = tHum;
    }
    else
    {
        HumidityX100 = 10000;
    }

    if(Temperature != NULL)
    {
        *Temperature = TemperatureX100;
    }

    if(Humidity != NULL)
    {
        *Humidity = HumidityX100;
    }

    printf("Traw: %x, Hraw: %x, T:%i, H:%i\n", TemperatureRaw, HumidityRaw, TemperatureX100, HumidityX100);
    return ESP_OK;
}