/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "nvs_flash.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_bt_defs.h"
#include "freertos/FreeRTOS.h"
#include "ble.h"
#include "i2c_wrapper.h"
#include "sth41.h"

static uint8_t BTH_Temp(uint8_t *buffer, int16_t temperature, uint16_t humidity)
{
    /* Temperature dimension: 0.01 ˚C */
    uint8_t i = 0;
    buffer[i++] = 0xD2;
    buffer[i++] = 0xFC;
    buffer[i++] = 0x40;
    buffer[i++] = 0x02;

    buffer[i++] = ((uint8_t*)&temperature)[0];
    buffer[i++] = ((uint8_t*)&temperature)[1];

    buffer[i++] = 0x03;

    buffer[i++] = ((uint8_t*)&humidity)[0];
    buffer[i++] = ((uint8_t*)&humidity)[1];
    return i;
}

static uint32_t counter;

void app_main(void)
{
    esp_err_t ret;
    uint16_t tt = 2506;
    uint8_t asd[16] = { 0xD2, 0xFC, 0x40, 0x3E, 0x01, 0x02, 0x03, 0x04};
    uint8_t length = 0;
    uint8_t serial[6];
    dtI2c_wrapper_transaction transaction1 = {.register_address = 0xFD, .writing = 1, .data = 0, .length = 0};
    dtI2c_wrapper_transaction transaction2 = {.register_address = 0xFD, .writing = 0, .data = serial, .length = 6};
    int16_t TemperatureX100;
    uint16_t HumidityX100;

    //initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    BLE_Init();
    I2C_Wrapper_Init();
    I2C_Wrapper_SetDevice(0x44, 100000);


    printf("Cycle starts\n");

    while(1)
    {
        /*if(ret == 0)
        {
            ret = I2C_Wrapper_Transmit(&transaction1);
            printf("i2c ret: %i\n", ret);
        }
        
        while(I2C_Wrapper_Transmit(&transaction2) != 0);
        printf("i2c ret: %i\n", ret);

        printf("response: %x %x %x %x %x %x\n", serial[0], serial[1], serial[2], serial[3], serial[4], serial[5]);*/

        SHT41_Measure(&TemperatureX100, &HumidityX100);
        printf("Temp: %i, Hum: %i\n", TemperatureX100, HumidityX100);

        counter++;
        tt++;

        length = BTH_Temp(asd, TemperatureX100, HumidityX100);
        BLE_RemoveServiceData();
        BLE_AddServiceData(asd, length);

        BLE_SendAdvertise();

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
