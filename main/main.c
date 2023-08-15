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

static const char *TAG = "MAIN";

#define I2C_MASTER_NUM        I2C_NUM_0
#define I2C_MASTER_SDA_IO     6
#define I2C_MASTER_SCL_IO     7
#define I2C_MASTER_FREQ_HZ    100000
#define I2C_MASTER_TIMEOUT_MS 1000

#define LED_GPIO 3

#define SLEEP_TIME_SECONDS 60

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

esp_err_t load_limits(Limits *limits) {
    // Öffne den NVS-Handle
    nvs_handle_t my_handle;
    esp_err_t    err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        return err;
    }

    // Laden Sie den Struct
    size_t required_size = sizeof(Limits);
    err = nvs_get_blob(my_handle, "limits", limits, &required_size);
    if (err != ESP_OK) {
        return err;
    }

    // Schließen
    nvs_close(my_handle);
    return ESP_OK;
}

void evaluate_limits(Limits *limits, double moisture, double temperature, double humidity, double pressure, double white, double visible) {
    int i = 0;
    if (moisture < limits->moisture.min || moisture > limits->moisture.max) {
        i = 1;
    } else if (temperature < limits->temperature.min || temperature > limits->temperature.max) {
        i = 1;
    } else if (humidity < limits->humidity.min || humidity > limits->humidity.max) {
        i = 1;
    } else if (pressure > limits->pressure.max || pressure < limits->pressure.min) {
        i = 1;
    } else if (white > limits->white.max || white < limits->white.min) {
        i = 1;
    } else if (visible > limits->visible.max || visible < limits->visible.min) {
        i = 1;
    }
    gpio_set_level(LED_GPIO, i);
}

void app_main(void) {
    Limits limits;
    // init WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    wifi_init_sta();

    // init I2C
    i2c_master_init();
    BME_init_wrapper();

    VEML_init();
    moisture_init();
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    double moisture = 50, white, visible, temperature, humidity, pressure;
    while (1) {
        ESP_LOGI(TAG, "UPDATING FKING SCESS");
        BME_force_read(&temperature, &pressure, &humidity);
        gpio_set_level(LED_GPIO, 0);
        VEML_read(&white, &visible);
        moisture_read(&moisture);
        send_data(&moisture, &temperature, &humidity, &pressure, &white, &visible);
        ESP_ERROR_CHECK_WITHOUT_ABORT(load_limits(&limits));
        evaluate_limits(&limits, moisture, temperature, humidity, pressure, white, visible);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
// vTaskDelay(1000 / portTICK_PERIOD_MS);
// esp_sleep_enable_timer_wakeup(SLEEP_TIME_SECONDS * 1000000LL);
// esp_deep_sleep_start();