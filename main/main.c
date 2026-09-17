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

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_bt_defs.h"
#include "freertos/FreeRTOS.h"

#define ADV_CONFIG_FLAG      (1 << 0)
#define SCAN_RSP_CONFIG_FLAG (1 << 1)
#define URI_PREFIX_HTTPS     (0x17)

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

static const char *DEMO_TAG = "BLE_BEACON";
static const char device_name[] = "BleThermometer";

static uint8_t adv_config_done = 0;
static esp_bd_addr_t local_addr;
static uint8_t local_addr_type;

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,  // 20ms
    .adv_int_max = 0x20,  // 20ms
    .adv_type = ADV_TYPE_SCAN_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

//configure raw data for advertising packet
static uint8_t adv_raw_data[] = {
    0x02, ESP_BLE_AD_TYPE_FLAG, 0x06,
    0x04, ESP_BLE_AD_TYPE_NAME_CMPL, 'M', 'y', 'C',
    0x02, ESP_BLE_AD_TYPE_TX_PWR, 0x09,
    0x03, ESP_BLE_AD_TYPE_APPEARANCE, 0x00,0x02,
    0x02, ESP_BLE_AD_TYPE_LE_ROLE, 0x00,
    0x09, ESP_BLE_AD_TYPE_SERVICE_DATA, 0xD2, 0xFC, 0x40, 0x3E, 0x01, 0x02, 0x03, 0x04,
};

static uint8_t temp_raw_data[31] = {};

static uint8_t scan_rsp_raw_data[] = {
    0x08, ESP_BLE_AD_TYPE_LE_DEV_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x11, ESP_BLE_AD_TYPE_URI, URI_PREFIX_HTTPS, '/', '/', 'e', 's', 'p', 'r', 'e', 's', 's', 'i', 'f', '.', 'c', 'o', 'm',
};

static uint8_t AppendData(esp_ble_adv_data_type ad_id, uint8_t *data, uint8_t length, uint8_t *buffer)
{
    uint8_t i = 0;
    uint8_t entry_length = length + 1; //payload + id byte

    if(data != NULL && (buffer != NULL))
    {
        while((i < 31) && (buffer[i] != 0))
        {
            i += buffer[i];
            i ++;
        }

        if((i + entry_length) <= 31)
        {
            buffer[i++] = entry_length;
            buffer[i++] = ad_id;
            memcpy(&buffer[i], data, length);
            i += length;
        }
        else
        {
            i = 0;
        }
    }
    return i;
}

static uint8_t ClearDataField(esp_ble_adv_data_type ap_id, uint8_t *buffer)
{
    uint8_t i = 0;
    if(buffer != NULL)
    {
        while((i < 30) && (buffer[i] != 0) && (buffer[i+1] != ap_id) && (buffer[i+1] != 0))
        {
            //printf("i: %i buffer[i]: %i\n", i, buffer[i]);
            i += buffer[i];
            i++;
            //printf("i: %i\n", i);
        }
        //printf("i: %i, left: %i\n", i, 31 - i - buffer[i] -1);
        if((buffer[i+1] == ap_id))
        {
            memmove(&buffer[i], &buffer[i+buffer[i]+1], 31 - i - buffer[i] -1);
        }
        memset(&buffer[i + buffer[i]], 0, 31 - (i + buffer[i]));
    }
    return i;
}

static void printBuff(uint8_t *buffer)
{
    uint8_t i = 0;
    while(i < 31 && (buffer[i] != 0))
    {
        int j;
        for(j = 0; j <= buffer[i]; j++)
        {
            printf("%X ", buffer[i + j]);
        }
        i += j;
        printf("\n");
    }
    while(i < 31)
    {
        printf("%X ", buffer[i]);
        i++;
    };
    printf("\n");
}

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

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(DEMO_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(DEMO_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    esp_bluedroid_config_t cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&cfg);
    if (ret) {
        ESP_LOGE(DEMO_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(DEMO_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret) {
        ESP_LOGE(DEMO_TAG, "gap register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gap_set_device_name(device_name);
    if (ret) {
        ESP_LOGE(DEMO_TAG, "set device name error, error code = %x", ret);
        return;
    }

    //config adv data
    adv_config_done |= ADV_CONFIG_FLAG;
    adv_config_done |= SCAN_RSP_CONFIG_FLAG;
    ret = esp_ble_gap_config_adv_data_raw(adv_raw_data, sizeof(adv_raw_data));
    if (ret) {
        ESP_LOGE(DEMO_TAG, "config adv data failed, error code = %x", ret);
        return;
    }

    ret = esp_ble_gap_get_local_used_addr(local_addr, &local_addr_type);
    if (ret) {
        ESP_LOGE(DEMO_TAG, "get local used address failed, error code = %x", ret);
        return;
    }

    scan_rsp_raw_data[2] = local_addr[5];
    scan_rsp_raw_data[3] = local_addr[4];
    scan_rsp_raw_data[4] = local_addr[3];
    scan_rsp_raw_data[5] = local_addr[2];
    scan_rsp_raw_data[6] = local_addr[1];
    scan_rsp_raw_data[7] = local_addr[0];
    ret = esp_ble_gap_config_scan_rsp_data_raw(scan_rsp_raw_data, sizeof(scan_rsp_raw_data));
    if (ret) {
        ESP_LOGE(DEMO_TAG, "config scan rsp data failed, error code = %x", ret);
    }
    uint16_t tt = 2506;
    uint8_t asd[] = { 0xD2, 0xFC, 0x40, 0x3E, 0x01, 0x02, 0x03, 0x04};
    uint8_t length = BTH_Temp(asd, tt);

    AppendData(ESP_BLE_AD_TYPE_SERVICE_DATA, asd, length, temp_raw_data);
    AppendData(ESP_BLE_AD_TYPE_SERVICE_DATA, asd, length, temp_raw_data);

    printBuff(temp_raw_data);
    for(int i = 0; i < 31; i++) temp_raw_data[i] = i;
    printBuff(temp_raw_data);

    memcpy(temp_raw_data, adv_raw_data, sizeof(adv_raw_data));
    temp_raw_data[sizeof(adv_raw_data)] = 0;
    printBuff(temp_raw_data);
    printf("Cycle starts\n");

    while(1)
    {
        counter++;
        adv_raw_data[sizeof(adv_raw_data) - 1] = (uint8_t) ((counter >> 24) & 0xFF);
        adv_raw_data[sizeof(adv_raw_data) - 2] = (uint8_t) ((counter >> 16) & 0xFF);
        adv_raw_data[sizeof(adv_raw_data) - 3] = (uint8_t) ((counter >> 8) & 0xFF);
        adv_raw_data[sizeof(adv_raw_data) - 4] = (uint8_t) ((counter >> 0) & 0xFF);
        //ret = esp_ble_gap_config_adv_data_raw(adv_raw_data, sizeof(adv_raw_data));
        if (ret) {
            ESP_LOGE(DEMO_TAG, "config adv data failed, error code = %x", ret);
            return;
        }
        tt++;
        length = BTH_Temp(asd, tt);

        printf("BTG_Temp: %i\n", length);
        ClearDataField(ESP_BLE_AD_TYPE_SERVICE_DATA, temp_raw_data);
        printBuff(temp_raw_data);
        
        AppendData(ESP_BLE_AD_TYPE_SERVICE_DATA, asd, length, temp_raw_data);
        ESP_LOGI(DEMO_TAG, "Value: %lu", counter);
        printBuff(temp_raw_data);
        ret = esp_ble_gap_config_adv_data_raw(temp_raw_data, sizeof(temp_raw_data));
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        ESP_LOGI(DEMO_TAG, "Advertising data set, status %d", param->adv_data_cmpl.status);
        adv_config_done &= (~ADV_CONFIG_FLAG);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        ESP_LOGI(DEMO_TAG, "Advertising data raw set, status %d", param->adv_data_raw_cmpl.status);
        adv_config_done &= (~ADV_CONFIG_FLAG);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        ESP_LOGI(DEMO_TAG, "Scan response data set, status %d", param->scan_rsp_data_cmpl.status);
        adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        ESP_LOGI(DEMO_TAG, "Scan response data raw set, status %d", param->scan_rsp_data_raw_cmpl.status);
        adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(DEMO_TAG, "Advertising start failed, status %d", param->adv_start_cmpl.status);
            break;
        }
        ESP_LOGI(DEMO_TAG, "Advertising start successfully");
        break;
    default:
        break;
    }
}
