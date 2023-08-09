#include "bme.h"

#include "bme280.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "BME_WRAPPER";

int8_t BME280_I2C_bus_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t cnt, void *interface) {
    int8_t  Error;
    uint8_t dev_addr = BME280_I2C_ADDR_PRIM;

    esp_err_t        espRc;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);

    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write(cmd, reg_data, cnt, true);
    i2c_master_stop(cmd);

    espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    if (espRc == ESP_OK) {
        Error = BME280_OK;
    } else {
        Error = BME280_E_COMM_FAIL;
    }
    i2c_cmd_link_delete(cmd);

    return Error;
}

int8_t BME280_I2C_bus_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t cnt, void *interface) {
    int8_t    Error;
    esp_err_t espRc;
    uint8_t   dev_addr = BME280_I2C_ADDR_PRIM;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);

    if (cnt > 1) {
        i2c_master_read(cmd, reg_data, cnt - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, reg_data + cnt - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    if (espRc == ESP_OK) {
        Error = BME280_OK;
    } else {
        Error = BME280_E_COMM_FAIL;
    }

    i2c_cmd_link_delete(cmd);

    return Error;
}

void BME280_delay_usek(uint32_t msek, void *interface) {
    vTaskDelay((msek / 1000) / portTICK_PERIOD_MS);
}

uint8_t           bme280_dev_addr = BME280_I2C_ADDR_PRIM;
struct bme280_dev dev = {
    .intf_ptr = &bme280_dev_addr,
    .intf = BME280_I2C_INTF,
    .read = BME280_I2C_bus_read,
    .write = BME280_I2C_bus_write,
    .delay_us = BME280_delay_usek};

void BME_init_wrapper() {
    int8_t rslt = BME280_OK;

    rslt = bme280_init(&dev);
    ESP_LOGD(TAG, "BME280 Init Result: %d", rslt);

    struct bme280_settings settings;

    rslt = bme280_get_sensor_settings(&settings, &dev);
    ESP_LOGD(TAG, "BME280 Get Sensor Settings Result: %d", rslt);

    settings.osr_h = BME280_OVERSAMPLING_1X;
    settings.osr_p = BME280_OVERSAMPLING_1X;
    settings.osr_t = BME280_OVERSAMPLING_1X;
    settings.filter = BME280_FILTER_COEFF_OFF;
    settings.standby_time = BME280_STANDBY_TIME_0_5_MS;

    rslt = bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &settings, &dev);
    ESP_LOGD(TAG, "BME280 Set Sensor Settings Result: %d", rslt);

    rslt = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &dev);
    ESP_LOGD(TAG, "BME280 Set Sensor Mode Result: %d", rslt);
}

int8_t BME_force_read(double *temperature, double *pressure, double *humidity) {
    int8_t   rslt = BME280_OK;
    uint32_t req_delay;

    struct bme280_data     comp_data;
    struct bme280_settings settings;

    settings.osr_h = BME280_OVERSAMPLING_2X;
    settings.osr_p = BME280_OVERSAMPLING_2X;
    settings.osr_t = BME280_OVERSAMPLING_2X;

    uint8_t settings_sel = BME280_SEL_OSR_TEMP | BME280_SEL_OSR_HUM | BME280_SEL_OSR_PRESS;

    rslt = bme280_set_sensor_settings(settings_sel, &settings, &dev);
    ESP_LOGD(TAG, "BME280 Set Sensor Settings Result: %d", rslt);

    rslt = bme280_cal_meas_delay(&req_delay, &settings);
    ESP_LOGD(TAG, "BME280 Cal Meas Delay Result: %d", rslt);

    rslt = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev);
    ESP_LOGD(TAG, "BME280 Set Sensor Mode Result: %d", rslt);

    dev.delay_us(req_delay, dev.intf_ptr);

    rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, &dev);
    ESP_LOGD(TAG, "BME280 Get Sensor Data Result: %d", rslt);

    *temperature = comp_data.temperature;
    *pressure = comp_data.pressure;
    *humidity = comp_data.humidity;

    return rslt;
}