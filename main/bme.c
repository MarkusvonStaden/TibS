#include "bme.h"

#include <rom/ets_sys.h>

#include "bme280.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"

// Constants and Macros
#define TAG             "BME_WRAPPER"
#define BME280_DEV_ADDR BME280_I2C_ADDR_PRIM

// BME280 configuration
uint8_t           bme280_dev_addr = BME280_I2C_ADDR_PRIM;
struct bme280_dev dev = {
    .intf_ptr = &bme280_dev_addr,
    .intf = BME280_I2C_INTF,
    .read = BME280_I2C_bus_read,
    .write = BME280_I2C_bus_write,
    .delay_us = BME280_delay_usek};

// Helper function to write data to BME280 using I2C
int8_t BME280_I2C_bus_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t cnt, void *interface) {
    int8_t           Error;
    esp_err_t        espRc;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Configure and start I2C write sequence
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BME280_DEV_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write(cmd, reg_data, cnt, true);
    i2c_master_stop(cmd);

    // Execute the write operation and handle the result
    espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    Error = (espRc == ESP_OK) ? BME280_OK : BME280_E_COMM_FAIL;

    i2c_cmd_link_delete(cmd);
    return Error;
}

// Helper function to read data from BME280 using I2C
int8_t BME280_I2C_bus_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t cnt, void *interface) {
    int8_t           Error;
    esp_err_t        espRc;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Configure and start I2C read sequence
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BME280_DEV_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BME280_DEV_ADDR << 1) | I2C_MASTER_READ, true);

    // Retrieve data
    if (cnt > 1) {
        i2c_master_read(cmd, reg_data, cnt - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, reg_data + cnt - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    // Execute the read operation and handle the result
    espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    Error = (espRc == ESP_OK) ? BME280_OK : BME280_E_COMM_FAIL;

    i2c_cmd_link_delete(cmd);
    return Error;
}

// Delay function for the BME280
void BME280_delay_usek(uint32_t msek, void *interface) {
    ets_delay_us(msek);
}

// Initialize BME280 sensor
void BME_init_wrapper() {
    int8_t                 rslt;
    struct bme280_settings settings;

    rslt = bme280_init(&dev);
    ESP_LOGI(TAG, "BME280 Init Result: %d", rslt);

    rslt = bme280_get_sensor_settings(&settings, &dev);
    ESP_LOGI(TAG, "BME280 Get Sensor Settings Result: %d", rslt);

    settings.osr_h = BME280_OVERSAMPLING_1X;
    settings.osr_p = BME280_OVERSAMPLING_1X;
    settings.osr_t = BME280_OVERSAMPLING_1X;
    settings.filter = BME280_FILTER_COEFF_OFF;
    settings.standby_time = BME280_STANDBY_TIME_0_5_MS;

    rslt = bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &settings, &dev);
    ESP_LOGI(TAG, "BME280 Set Sensor Settings Result: %d", rslt);

    rslt = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &dev);
    ESP_LOGI(TAG, "BME280 Set Sensor Mode Result: %d", rslt);
}

// Read data from BME280 sensor
int8_t BME_force_read(double *temperature, double *pressure, double *humidity) {
    int8_t                 rslt;
    uint32_t               req_delay;
    struct bme280_data     comp_data;
    struct bme280_settings settings;

    settings.osr_h = BME280_OVERSAMPLING_2X;
    settings.osr_p = BME280_OVERSAMPLING_2X;
    settings.osr_t = BME280_OVERSAMPLING_2X;

    uint8_t settings_sel = BME280_SEL_OSR_TEMP | BME280_SEL_OSR_HUM | BME280_SEL_OSR_PRESS;

    rslt = bme280_set_sensor_settings(settings_sel, &settings, &dev);
    ESP_LOGI(TAG, "BME280 Set Sensor Settings Result: %d", rslt);

    rslt = bme280_cal_meas_delay(&req_delay, &settings);
    ESP_LOGI(TAG, "BME280 Cal Meas Delay Result: %d", rslt);

    rslt = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev);
    ESP_LOGI(TAG, "BME280 Set Sensor Mode Result: %d", rslt);

    dev.delay_us(req_delay, dev.intf_ptr);

    rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, &dev);
    ESP_LOGI(TAG, "BME280 Get Sensor Data Result: %d", rslt);

    *temperature = comp_data.temperature;
    *pressure = comp_data.pressure;
    *humidity = comp_data.humidity;

    return rslt;
}
