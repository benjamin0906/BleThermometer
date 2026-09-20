idf_component_register(SRCS "main.c" "ble.c" "i2c_wrapper.c" "sht41.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash bt esp_driver_i2c)
