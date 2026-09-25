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
#include "driver/gpio.h"
#include "adc.h"
#include "esp_timer.h"

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
    /*gpio_config_t debug_pin = {
        .intr_type = GPIO_INTR_DISABLE, 
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GPIO_NUM_21),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE};
    gpio_config(&debug_pin);
    gpio_set_level(GPIO_NUM_21, 1); //debug purpose*/
    esp_err_t ret;
    uint8_t service_payload[16] = { 0xD2, 0xFC, 0x40, 0x3E, 0x01, 0x02, 0x03, 0x04};
    uint8_t serv_payload_len = 0;
    int16_t TemperatureX100;
    uint16_t HumidityX100;
    uint16_t adc_result_num = 0;
    uint16_t adc_result[4];

    //initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //RTC_SLOW_CLOCK;

    esp_sleep_enable_timer_wakeup(120000000);
    //esp_sleep_enable_timer_wakeup(20000000);

    adc_init();
    BLE_Init();
    I2C_Wrapper_Init();
    I2C_Wrapper_SetDevice(0x44, 100000);

    uint64_t timestamp = 0;
    uint16_t sample = 0;

    while(1)
    {
        SHT41_Measure(&TemperatureX100, &HumidityX100);
        //printf("Temp: %i, Hum: %i\n", TemperatureX100, HumidityX100);

        
        timestamp = esp_timer_get_time();
        sample = adc_get_raw(ADC_CHANNEL_0);
        timestamp = esp_timer_get_time() - timestamp;
        printf("Td: %llu, result: %i\n", timestamp, sample);
        

        serv_payload_len = BTH_Temp(service_payload, TemperatureX100, HumidityX100);
        BLE_RemoveServiceData();
        BLE_AddServiceData(service_payload, serv_payload_len);

        BLE_StartAdvertise();
        while(BLE_AdvStatus() == 0);
        //printf("Adv result1: %i\n", BLE_AdvStatus());

        vTaskDelay(500 / portTICK_PERIOD_MS);

        BLE_StopAdverting();
        while(BLE_AdvStatus() != 0);
        //printf("Adv result2: %i\n", BLE_AdvStatus());

        //gpio_set_level(GPIO_NUM_21, 0); //debug purpose
        //esp_deep_sleep_start();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
