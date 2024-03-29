#include "veml.h"

#include <stdio.h>

#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"

// Constants and Macros
#define I2C_MASTER_NUM I2C_NUM_0
#define MULTIPLIER     0.00213
#define VEML_DEV_ADDR  0x10
#define VEML_DG_X2     = (0x01 << 4)
#define VEML_GAIN_X4   = (0x03 << 2)

static const char *TAG = "VEML";

// Helper function to write data to VEML using I2C
esp_err_t VEML_I2C_write(uint8_t reg_addr, uint8_t *reg_data, uint32_t len) {
    esp_err_t        ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (VEML_DEV_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write(cmd, reg_data, len, true);
    i2c_master_stop(cmd);

    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

// Helper function to read data from VEML using I2C
esp_err_t VEML_I2C_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len) {
    esp_err_t        ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (VEML_DEV_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (VEML_DEV_ADDR << 1) | I2C_MASTER_READ, true);

    if (len > 1) {
        i2c_master_read(cmd, reg_data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, reg_data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

// Initialize VEML sensor
esp_err_t VEML_init() {
    uint8_t data[2] = {0x00, 0b00011101};
    return VEML_I2C_write(0x00, data, 2);
}

// Read data from VEML sensor
esp_err_t VEML_read(double *white, double *visible) {
    uint8_t  data[2];
    uint16_t value;

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
