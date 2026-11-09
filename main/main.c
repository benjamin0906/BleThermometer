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

#define DEBUG_PIN
#define SLEEP_TIME (180000000)

static const uint16_t adc_scalefactor_nom = 1000;
static const uint16_t adc_scalefactor_denom = 641;

static uint8_t BTH_Temp(uint8_t *buffer, int16_t temperature, uint16_t humidity, uint8_t battery)
{
    /* Temperature dimension: 0.01 ˚C */
    uint8_t i = 0;
    buffer[i++] = 0xD2;
    buffer[i++] = 0xFC;
    buffer[i++] = 0x40;

    buffer[i++] = 0x01;
    buffer[i++] = battery;

    buffer[i++] = 0x02;
    buffer[i++] = ((uint8_t*)&temperature)[0];
    buffer[i++] = ((uint8_t*)&temperature)[1];

    buffer[i++] = 0x03;
    buffer[i++] = ((uint8_t*)&humidity)[0];
    buffer[i++] = ((uint8_t*)&humidity)[1];

    return i;
}

static uint8_t SocCalculation(uint16_t Voltage)
{
    const uint16_t SoC_Volt_map[] = {3200, 3600, 3700, 3750, 3790, 3830, 3870, 3920, 4000, 4100, 4200};
    uint8_t SoC = 0;
    uint8_t i = 0;
    uint8_t lower = 0;
    uint8_t higher = 0;
    while((i < (sizeof(SoC_Volt_map)/sizeof(SoC_Volt_map[0]))) && (Voltage > SoC_Volt_map[i])) i++;
    //printf("Voltage: %i, i: %i \n", Voltage, i);
    if((i > 1) && (i < (sizeof(SoC_Volt_map)/sizeof(SoC_Volt_map[0]))))
    {
        lower = (i - 1) * 10;
        higher = i * 10;
        SoC = (((uint16_t)(higher - lower) * (Voltage - SoC_Volt_map[i-1])) / (SoC_Volt_map[i] - SoC_Volt_map[i-1])) + lower;
    }
    else if(i > 1)
    {
        SoC = 101;
    }
    return SoC;
}

void app_main(void)
{
#if defined(DEBUG_PIN)
    gpio_config_t debug_pin = {
        .intr_type = GPIO_INTR_DISABLE, 
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GPIO_NUM_21),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE};
    gpio_config(&debug_pin);
    gpio_set_level(GPIO_NUM_21, 1); //debug purpose*/
#endif
    esp_err_t ret;
    uint8_t service_payload[16] = { 0xD2, 0xFC, 0x40, 0x3E, 0x01, 0x02, 0x03, 0x04};
    uint8_t serv_payload_len = 0;
    int16_t TemperatureX100;
    uint16_t HumidityX100;

    //initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_sleep_enable_timer_wakeup(SLEEP_TIME);

    adc_init();
    adc_ConfigConversion(ADC_CHANNEL_0);
    BLE_Init();
    I2C_Wrapper_Init();
    I2C_Wrapper_SetDevice(0x44, 100000);

    uint16_t sampleV = 0;

    while(1)
    {
        adc_TriggerOneshot();
        SHT41_Measure(&TemperatureX100, &HumidityX100);
        
        while(adc_IsOneshotDone() == 0);
        sampleV = adc_GetLastVoltage();

        serv_payload_len = BTH_Temp(service_payload, TemperatureX100, HumidityX100, SocCalculation(((uint32_t)sampleV * adc_scalefactor_nom) / adc_scalefactor_denom));
        BLE_RemoveServiceData();
        BLE_AddServiceData(service_payload, serv_payload_len);

        BLE_StartAdvertise();
        while(BLE_AdvStatus() == 0);
        //printf("Adv result1: %i\n", BLE_AdvStatus());

        vTaskDelay(500 / portTICK_PERIOD_MS);

        BLE_StopAdverting();
        while(BLE_AdvStatus() != 0);

#if defined(DEBUG_PIN)
        gpio_set_level(GPIO_NUM_21, 0);
#endif
        esp_deep_sleep_start();
        //vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
