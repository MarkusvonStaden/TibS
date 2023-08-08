#include <stdio.h>

// #include "bme.h"
// #include "bme280.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_MASTER_NUM        I2C_NUM_0
#define I2C_MASTER_SDA_IO     6
#define I2C_MASTER_SCL_IO     7
#define I2C_MASTER_FREQ_HZ    100000
#define I2C_MASTER_TIMEOUT_MS 1000

#define PIN_OUTPUT 5
#define PIN_INPUT  4

static const char* TAG = "MAIN";

static int64_t start_time;

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    int64_t end_time = esp_timer_get_time();
    ESP_LOGI(TAG, "Pin %d is HIGH. Time taken: %lld microseconds.", PIN_INPUT, end_time - start_time);
}

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

static void gpio_configuration(void) {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1ULL << PIN_OUTPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = 1ULL << PIN_INPUT;
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_INPUT, gpio_isr_handler, NULL);
}

void app_main(void) {
    gpio_configuration();
    gpio_set_level(PIN_OUTPUT, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    while (1) {
        gpio_set_level(PIN_OUTPUT, 0);
        vTaskDelay(100);
        gpio_set_level(PIN_OUTPUT, 1);
        ESP_LOGI(TAG, "Pin %d is set to HIGH.", PIN_OUTPUT);

        start_time = esp_timer_get_time();

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
