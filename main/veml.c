#include "veml.h"

#include <stdio.h>

#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"

#define I2C_MASTER_NUM I2C_NUM_0

#define MULTIPLIER 0.27264

static const char *TAG = "VEML";

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

esp_err_t VEML_init() {
    uint8_t data[2] = {0x00, 0x00};
    return VEML_I2C_write(0x00, data, 2);
}

esp_err_t VEML_read(double *white, double *visible) {
    uint8_t   data[2];
    uint16_t  value;
    esp_err_t ret = VEML_I2C_read(0x04, data, 2);
    if (ret != ESP_OK) {
        return ret;
    }
    value = (data[1] << 8) | data[0];
    *white = value * MULTIPLIER;

    ret = VEML_I2C_read(0x05, data, 2);
    if (ret != ESP_OK) {
        return ret;
    }
    value = (data[1] << 8) | data[0];
    *visible = value * MULTIPLIER;

    return ret;
}