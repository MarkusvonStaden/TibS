#include <stdio.h>

#include "bme.h"
#include "bme280.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "moisture.h"
#include "nvs_flash.h"
#include "veml.h"
#include "wifi.h"

// Constants and Macros
static const char *TAG = "MAIN";
static const char *VERSION = "1.0.8";
#define I2C_MASTER_NUM        I2C_NUM_0
#define I2C_MASTER_SDA_IO     6
#define I2C_MASTER_SCL_IO     7
#define I2C_MASTER_FREQ_HZ    100000
#define I2C_MASTER_TIMEOUT_MS 1000

#define LED_GPIO           3
#define SLEEP_TIME_SECONDS 3600

// Function to initialize I2C master
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

// Function to load limits
esp_err_t load_limits(Limits *limits) {
    // Open the NVS handle
    nvs_handle_t my_handle;
    esp_err_t    err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        return err;
    }

    // Load the struct
    size_t required_size = sizeof(Limits);
    err = nvs_get_blob(my_handle, "limits", limits, &required_size);

    // Close the handle
    nvs_close(my_handle);
    return err;
}

// Function to evaluate limits
void evaluate_limits(Limits *limits, double moisture, double temperature, double humidity, double pressure, double white, double visible) {
    int alert = 0;

    if (moisture < limits->moisture.min || moisture > limits->moisture.max) alert = 1;
    if (temperature < limits->temperature.min || temperature > limits->temperature.max) alert = 1;
    if (humidity < limits->humidity.min || humidity > limits->humidity.max) alert = 1;
    if (pressure < limits->pressure.min || pressure > limits->pressure.max) alert = 1;
    if (white < limits->white.min || white > limits->white.max) alert = 1;
    if (visible < limits->visible.min || visible > limits->visible.max) alert = 1;

    ESP_LOGI(TAG, "Alert: %d", alert);

    gpio_set_level(LED_GPIO, alert);
    ESP_ERROR_CHECK(gpio_hold_en(LED_GPIO));
    gpio_deep_sleep_hold_en();
}

// Main application function
void app_main(void) {
    Limits limits;

    // Initialize WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    wifi_init_sta();

    // Initialize I2C
    i2c_master_init();
    BME_init_wrapper();

    // Initialize other components
    VEML_init();
    moisture_init();

    gpio_deep_sleep_hold_dis();
    gpio_hold_dis(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    double moisture = 50, white, visible, temperature, humidity, pressure;

    BME_force_read(&temperature, &pressure, &humidity);
    moisture_read(&moisture);
    gpio_set_level(LED_GPIO, 0);
    VEML_read(&white, &visible);

    send_data(&moisture, &temperature, &humidity, &pressure, &white, &visible, VERSION);
    load_limits(&limits);

    evaluate_limits(&limits, moisture, temperature, humidity, pressure, white, visible);

    // Sleeping code
    esp_sleep_enable_timer_wakeup(SLEEP_TIME_SECONDS * 1000000LL);
    esp_deep_sleep_start();
}
