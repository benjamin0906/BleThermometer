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

static uint8_t BTH_Temp(uint8_t *buffer, int16_t temperature)
{
    /* Temperature dimension: 0.01 ˚C */
    uint8_t i = 0;
    buffer[i++] = 0xD2;
    buffer[i++] = 0xFC;
    buffer[i++] = 0x40;
    buffer[i++] = 0x02;

    buffer[i++] = ((uint8_t*)&temperature)[0];
    buffer[i++] = ((uint8_t*)&temperature)[1];
    return i;
}

static uint32_t counter;

void app_main(void)
{
    esp_err_t ret;

    //initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    BLE_Init();

    uint16_t tt = 2506;
    uint8_t asd[] = { 0xD2, 0xFC, 0x40, 0x3E, 0x01, 0x02, 0x03, 0x04};
    uint8_t length = BTH_Temp(asd, tt);

    printf("Cycle starts\n");

    while(1)
    {
        counter++;
        tt++;
        length = BTH_Temp(asd, tt);

        BLE_AddServiceData(asd, length);
        BLE_SendAdvertise();

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
