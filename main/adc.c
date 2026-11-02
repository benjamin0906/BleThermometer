#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "hal/adc_ll.h"
#include "hal/adc_oneshot_hal.h"
#include "esp_clk_tree.h"
#include "esp_private/adc_share_hw_ctrl.h"
#include "esp_intr_alloc.h"
#include "soc/interrupts.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define ADC1_CHAN0          ADC_CHANNEL_0
#define ADC_ATTEN           ADC_ATTEN_DB_12
#define ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED 1
#define ADC_SAMPLE_CYCLE    7
#define ADC_SAR_CLK_DIV 8
#define ADC_CTRL_CLK_DIV_NUM    39

static const char *TAG = "adc_example";
adc_cali_handle_t adc1_cali_chan0_handle = NULL;
adc_oneshot_unit_handle_t handle;
static adc_oneshot_hal_ctx_t adc_hal;
static intr_handle_t adc_intr_handle;
static SemaphoreHandle_t adc_done_semaph;
static uint16_t adc_result;

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

static void adc_isr(void *arg)
{
    BaseType_t task_woken = pdFALSE;

    if((APB_SARADC.int_st.val & ADC_LL_EVENT_ADC1_ONESHOT_DONE) == 0)
    {
        return;
    }
    adc_result = adc_oneshot_ll_get_raw_result(ADC_UNIT_1);
    adc_oneshot_ll_clear_event(ADC_LL_EVENT_ADC1_ONESHOT_DONE);
    adc_oneshot_ll_start(false);

    xSemaphoreGiveFromISR(adc_done_semaph, &task_woken);
    if(task_woken)
    {
        portYIELD_FROM_ISR();
    }
}

void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config1 = {.unit_id = ADC_UNIT_1,};
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    uint32_t clk_src_freq_hz = 0;
    adc_oneshot_hal_cfg_t hal_config = {
        .unit = ADC_UNIT_1,
        .work_mode = ADC_HAL_SINGLE_READ_MODE,
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,
        .clk_src_freq_hz = clk_src_freq_hz,
    };
    adc_oneshot_hal_chan_cfg_t hal_chan_config = {
        .atten = ADC_ATTEN,
        .bitwidth = SOC_ADC_RTC_MAX_BITWIDTH,
    };

    adc_oneshot_new_unit(&init_config1, &handle);

    adc_oneshot_config_channel(handle, ADC1_CHAN0, &config);

    esp_clk_tree_src_get_freq_hz(ADC_DIGI_CLK_SRC_DEFAULT, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &clk_src_freq_hz);
    adc_oneshot_hal_init(&adc_hal, &hal_config);    
    adc_oneshot_hal_channel_config(&adc_hal, &hal_chan_config, ADC1_CHAN0);

    adc_done_semaph = xSemaphoreCreateBinary();
    esp_intr_alloc(ETS_APB_ADC_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1, adc_isr, NULL, &adc_intr_handle);
    adc_oneshot_ll_clear_event(ADC_LL_EVENT_ADC1_ONESHOT_DONE);
    APB_SARADC.int_ena.adc1_done = 1;

    adc_hal_calibration_init(ADC_UNIT_1);
    adc_set_hw_calibration_code(ADC_UNIT_1, ADC_ATTEN);
}

void adc_ConfigConversion(adc_channel_t ch)
{
    adc_oneshot_ll_disable_all_unit();
    xSemaphoreTake(adc_done_semaph, 0);
    adc_oneshot_hal_setup(&adc_hal, ch);
    adc_ll_digi_controller_clk_div(ADC_CTRL_CLK_DIV_NUM, ADC_LL_CLKM_DIV_B_DEFAULT, ADC_LL_CLKM_DIV_A_DEFAULT);
    adc_ll_digi_set_clk_div(ADC_SAR_CLK_DIV);

    adc_calibration_init(ADC_UNIT_1, ch, ADC_ATTEN, &adc1_cali_chan0_handle);

    adc_oneshot_ll_clear_event(ADC_LL_EVENT_ADC1_ONESHOT_DONE);
    adc_oneshot_ll_enable(ADC_UNIT_1);
}

void adc_TriggerOneshot(void)
{
    xSemaphoreTake(adc_done_semaph, 0);
    adc_oneshot_ll_start(true);
}

uint8_t adc_IsOneshotDone(void)
{
    return xSemaphoreTake(adc_done_semaph, 0);
}

uint16_t adc_GetLastRaw(void)
{
    return adc_result;
}

uint16_t adc_GetLastVoltage(void)
{
    int voltage = 0;
    adc_cali_raw_to_voltage(adc1_cali_chan0_handle, adc_result, &voltage);
    return (uint16_t)voltage;
}

