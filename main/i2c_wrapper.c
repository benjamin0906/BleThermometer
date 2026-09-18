#include "driver/i2c_master.h"
#include "i2c_wrapper.h"

static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;

void I2C_Wrapper_Init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_8,
        .scl_io_num = GPIO_NUM_9,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    i2c_new_master_bus(&bus_config, &bus_handle);
}

void I2C_Wrapper_SetDevice(uint8_t SlaveAddress, uint32_t freq)
{
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SlaveAddress,
        .scl_speed_hz = freq,
    };
    i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
}

esp_err_t I2C_Wrapper_Transmit(dtI2c_wrapper_transaction *const transaction)
{
    esp_err_t ret = ESP_FAIL;
    if(transaction != NULL)
    {
        ret = ESP_OK;
        if(transaction->writing != 0)
        {
            i2c_master_transmit_multi_buffer_info_t tx_buffs[2] = {
                {.write_buffer = &transaction->register_address, .buffer_size = 1},
                {.write_buffer = transaction->data, .buffer_size = transaction->length}};
            ret = i2c_master_multi_buffer_transmit(dev_handle, tx_buffs, 2, 500);
        }
        else
        {
            if((transaction->data != NULL) && (transaction->length != 0))
            {
                if(transaction->register_length != 0)
                {
                    ret = i2c_master_transmit_receive(dev_handle, &transaction->register_address, transaction->register_length, transaction->data, transaction->length, 500);
                }
                else
                {
                    ret = i2c_master_receive(dev_handle, transaction->data, transaction->length, 500);
                }
            }
        }
    }
    return ret;
}
