#include <stdio.h>

#include "bme.h"
#include "bme280.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"

#define I2C_MASTER_NUM        I2C_NUM_0
#define I2C_MASTER_SDA_IO     6
#define I2C_MASTER_SCL_IO     7
#define I2C_MASTER_FREQ_HZ    100000
#define I2C_MASTER_TIMEOUT_MS 1000

static const char *TAG = "MAIN";

static esp_err_t i2c_master_init() {
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ};
    i2c_param_config(I2C_MASTER_NUM, &i2c_config);
    return i2c_driver_install(I2C_MASTER_NUM, i2c_config.mode, 0, 0, 0);
}

void app_main(void) {
    int8_t rslt;
    i2c_master_init();
    BME_init_wrapper();

    struct bme280_data comp_data;

    while (1) {
        rslt = BME_force_read(&comp_data);
        ESP_LOGD(TAG, "Force read result: %d", rslt);

        ESP_LOGI(TAG, "Temperature: %f degC, Pressure: %f Pa, Humidity: %f", comp_data.temperature, comp_data.pressure, comp_data.humidity);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
