#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi.h"

static const char *TAG = "MAIN";

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "Hello world!");
    wifi_init_sta();

    uint32_t temperature = 0;
    uint32_t humidity = 0;
    uint32_t pressure = 0;

    ESP_LOGI(TAG, "Sending data...");
    send_data(&temperature, &humidity, &pressure);

    ESP_LOGI(TAG, "Bye world!");
}
