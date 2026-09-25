#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_timer.h"

#define ADC1_CHAN0          ADC_CHANNEL_0
#define ADC_ATTEN           ADC_ATTEN_DB_12
#define ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED 1

static const char *TAG = "adc_example";
adc_cali_handle_t adc1_cali_chan0_handle = NULL;
adc_continuous_handle_t handle;
uint64_t ts;
uint64_t diff;

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    uint64_t timestamp = esp_timer_get_time();
    diff = timestamp - ts;
    ts = timestamp;
    return 0;
}

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

void adc_init(void)
{
    adc_continuous_handle_cfg_t adc_config = {.max_store_buf_size = 16, .conv_frame_size = SOC_ADC_DIGI_RESULT_BYTES * 2};
    adc_continuous_config_t dig_cfg = {.sample_freq_hz = 20000, .conv_mode = ADC_CONV_SINGLE_UNIT_1};
    adc_digi_pattern_config_t adc_pattern = {
        .atten = ADC_ATTEN,
        .channel = ADC1_CHAN0,
        .unit = ADC_UNIT_1,
        .bit_width = SOC_ADC_DIGI_MAX_BITWIDTH};
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    esp_err_t ret;

    
    dig_cfg.adc_pattern = &adc_pattern;
    dig_cfg.pattern_num = 1;

    ret = adc_continuous_new_handle(&adc_config, &handle);
    printf("ret 1: %i\n", ret);
    ret = adc_continuous_config(handle, &dig_cfg);
    printf("ret 2: %i\n", ret);
    ret = adc_calibration_init(ADC_UNIT_1, ADC1_CHAN0, ADC_ATTEN, &adc1_cali_chan0_handle);
    printf("ret 3: %i\n", ret);
    ret = adc_continuous_register_event_callbacks(handle, &cbs, NULL);
    printf("ret 4: %i\n", ret);
    adc_ll
}

void adc_cont_start(void)
{
    adc_continuous_start(handle);
}

void adc_cont_stop(void)
{
    adc_continuous_stop(handle);
}

uint32_t adc_get_raw(adc_channel_t ch, uint16_t* buff, uint16_t max_len)
{
    uint32_t ret = 0;
    printf("diff: %llu\n", diff);
    if((buff == NULL) || (ESP_OK != adc_continuous_read(handle, (uint8_t*)buff, max_len, &ret, 0)))
    {
        ret = 0;
    }
    return ret;
}

uint16_t adc_get_volt(adc_channel_t ch)
{
    int voltage = 0;
    /*uint16_t raw = adc_get_raw(ch);
    if(ch == ADC1_CHAN0)
    {
        adc_cali_raw_to_voltage(adc1_cali_chan0_handle, raw, &voltage);
    }*/
    
    return (uint16_t)voltage;
}

