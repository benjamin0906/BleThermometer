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
#include "esp_sleep.h"
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

void app_main(void)
{
    esp_err_t ret;
    uint8_t service_payload[16] = { 0xD2, 0xFC, 0x40, 0x3E, 0x01, 0x02, 0x03, 0x04};
    uint8_t serv_payload_len = 0;
    int16_t TemperatureX100;
    uint16_t HumidityX100;
    uint16_t counter = 0;

    //initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //RTC_SLOW_CLOCK;

    esp_sleep_enable_timer_wakeup(20000000);

    BLE_Init();
    I2C_Wrapper_Init();
    I2C_Wrapper_SetDevice(0x44, 100000);

    while(1)
    {
        SHT41_Measure(&TemperatureX100, &HumidityX100);
        printf("Temp: %i, Hum: %i\n", TemperatureX100, HumidityX100);

        serv_payload_len = BTH_Temp(service_payload, TemperatureX100, HumidityX100);
        BLE_RemoveServiceData();
        BLE_AddServiceData(service_payload, serv_payload_len);

        BLE_StartAdvertise();
        while(BLE_AdvStatus() == 0);
        printf("Adv result1: %i\n", BLE_AdvStatus());

        vTaskDelay(500 / portTICK_PERIOD_MS);

        BLE_StopAdverting();
        while(BLE_AdvStatus() != 0);
        printf("Adv result2: %i\n", BLE_AdvStatus());
        
        fflush(stdout);
        if(counter < 2)
        {
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            counter++;
        }
        else
        {
            esp_light_sleep_start();
        }
    }
}
