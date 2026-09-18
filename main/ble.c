#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_log.h"

#define ADV_CONFIG_FLAG      (1 << 0)
#define SCAN_RSP_CONFIG_FLAG (1 << 1)

static const char *TAG = "BLE";
static const char device_name[] = "BleThermometer";
static uint8_t adv_config_done = 0;

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,  // 20ms
    .adv_int_max = 0x20,  // 20ms
    .adv_type = ADV_TYPE_SCAN_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static uint8_t adv_raw_data[31] = {
    0x02, ESP_BLE_AD_TYPE_FLAG, 0x06,
    0x04, ESP_BLE_AD_TYPE_NAME_CMPL, 'M', 'y', 'C',
    0x02, ESP_BLE_AD_TYPE_TX_PWR, 0x09,
    0x03, ESP_BLE_AD_TYPE_APPEARANCE, 0x00,0x02,
    0x02, ESP_BLE_AD_TYPE_LE_ROLE, 0x00,
    0x09, ESP_BLE_AD_TYPE_SERVICE_DATA, 0xD2, 0xFC, 0x40, 0x3E, 0x01, 0x02, 0x03, 0x04,
};

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

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
    uint8_t ret = 0;
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
            ret = 1;
            memmove(&buffer[i], &buffer[i+buffer[i]+1], 31 - i - buffer[i] -1);
        }
        memset(&buffer[i + buffer[i]], 0, 31 - (i + buffer[i]));
    }
    return ret;
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


void BLE_Init(void)
{
    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bluedroid_config_t cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    int8_t tx_power_map[] = {-24, -21, -18, -15, -12, -9, -6, -3, 0, 3, 6, 9, 12, 15, 18, 20};
    uint8_t tx_power_index;

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init_with_cfg(&cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret) {
        ESP_LOGE(TAG, "gap register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gap_set_device_name(device_name);
    if (ret) {
        ESP_LOGE(TAG, "set device name error, error code = %x", ret);
        return;
    }

    adv_config_done |= ADV_CONFIG_FLAG;
    
    ret = esp_ble_gap_config_adv_data_raw(adv_raw_data, sizeof(adv_raw_data));
    if (ret) {
        ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
        return;
    }

    if(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P20) == ESP_OK)
    {
        ESP_LOGI(TAG, "Tx power has been set");
    }
    else
    {
        ESP_LOGI(TAG, "Tx power has NOT been set");
    }

    tx_power_index = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);
    if(tx_power_index != 0xFF)
    {
        ESP_LOGI(TAG, "Tx power: %i", tx_power_map[tx_power_index]);
        ClearDataField(ESP_BLE_AD_TYPE_TX_PWR, adv_raw_data);
        AppendData(ESP_BLE_AD_TYPE_TX_PWR, (uint8_t*)&tx_power_map[tx_power_index], 1, adv_raw_data);
    }
}

void BLE_SendAdvertise(void)
{
    esp_ble_gap_config_adv_data_raw(adv_raw_data, sizeof(adv_raw_data));
}

void BLE_AddServiceData(uint8_t *data, uint8_t length)
{
    AppendData(ESP_BLE_AD_TYPE_SERVICE_DATA, data, length, adv_raw_data);
    printBuff(adv_raw_data);
}

void BLE_RemoveServiceData(void)
{
    while(ClearDataField(ESP_BLE_AD_TYPE_SERVICE_DATA, adv_raw_data) != 0);
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Advertising data set, status %d", param->adv_data_cmpl.status);
        adv_config_done &= (~ADV_CONFIG_FLAG);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Advertising data raw set, status %d", param->adv_data_raw_cmpl.status);
        adv_config_done &= (~ADV_CONFIG_FLAG);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Scan response data set, status %d", param->scan_rsp_data_cmpl.status);
        adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Scan response data raw set, status %d", param->scan_rsp_data_raw_cmpl.status);
        adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising start failed, status %d", param->adv_start_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "Advertising start successfully");
        break;
    case ESP_GAP_BLE_GET_DEV_NAME_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_GET_DEV_NAME_COMPLETE_EVT has come");
        break;
    default:
        break;
    }
}