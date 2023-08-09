#include <stdio.h>

#include "bme.h"
#include "bme280.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi.h"

static const char *TAG = "MAIN";

#define I2C_MASTER_NUM        I2C_NUM_0
#define I2C_MASTER_SDA_IO     6
#define I2C_MASTER_SCL_IO     7
#define I2C_MASTER_FREQ_HZ    100000
#define I2C_MASTER_TIMEOUT_MS 1000

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

esp_err_t VEML_I2C_write(uint8_t reg_addr, uint8_t *reg_data, uint32_t len) {
    uint8_t   dev_addr = 0x10;
    esp_err_t ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write(cmd, reg_data, len, true);
    i2c_master_stop(cmd);

    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 10 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        return ret;
    }
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t VEML_I2C_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len) {
    uint8_t   dev_addr = 0x10;
    esp_err_t ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);

    if (len > 1) {
        i2c_master_read(cmd, reg_data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, reg_data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 10 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        return ret;
    }
    i2c_cmd_link_delete(cmd);
    return ret;
}

void app_main(void) {
    // init WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "Hello world!");
    wifi_init_sta();

    // init I2C
    int8_t rslt;
    i2c_master_init();
    BME_init_wrapper();

    uint8_t data[2] = {0x00, 0x00};
    VEML_I2C_write(0x00, data, 2);

    uint16_t value;
    double   lux;

    while (1) {
        double temperature, humidity, pressure;

        while (1) {
            rslt = BME_force_read(&temperature, &pressure, &humidity);
            ESP_LOGD(TAG, "Force read result: %d", rslt);

            ESP_LOGI(TAG, "Temperature: %f degC, Pressure: %f Pa, Humidity: %f", comp_data.temperature, comp_data.pressure, comp_data.humidity);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }