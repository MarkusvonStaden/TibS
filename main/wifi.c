#include "wifi.h"

#include <string.h>

#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

// Constants and Macros
#define SLEEP_TIME_SECONDS 10

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MESSAGE_SENT_BIT   BIT0
#define MAXIMUM_RETRY      5

#define URL          BASE_URL "/api/measurements"
#define FIRMWARE_URL BASE_URL "/api/firmwareupdate"

#define BUFFSIZE 1024

static const char*        TAG = "WiFi";
static EventGroupHandle_t s_wifi_event_group;
static int                s_retry_num = 0;

// Forward declarations
void getUpdate(void);

// Function to save limits
esp_err_t save_limits(const Limits* limits) {
    esp_err_t err;

    // Open the NVS handle
    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        return err;
    }

    // Save the struct
    err = nvs_set_blob(my_handle, "limits", limits, sizeof(Limits));
    if (err != ESP_OK) {
        return err;
    }

    // Commit and close
    err = nvs_commit(my_handle);
    nvs_close(my_handle);

    ESP_LOGI(TAG, "Limits saved to NVS");
    return err;
}

// Event handler for WiFi events
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Failed connecting to WiFi. Retrying...");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Connected to IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Initialize WiFi in STA mode
void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    // WiFi Configuration
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta finished. Wait for connection");
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP");
}

// Event handler for HTTP events
esp_err_t http_client_event_handler(esp_http_client_event_handle_t evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            Limits limits = {0};

            cJSON* root = cJSON_Parse(evt->data);

            cJSON* data_limits = cJSON_GetObjectItem(root, "limits");

            cJSON* moisture = cJSON_GetObjectItem(data_limits, "moisture");
            if (moisture) {
                limits.moisture.min = cJSON_GetArrayItem(moisture, 0)->valuedouble;
                limits.moisture.max = cJSON_GetArrayItem(moisture, 1)->valuedouble;
            }

            cJSON* temperature = cJSON_GetObjectItem(data_limits, "temperature");
            if (temperature) {
                limits.temperature.min = cJSON_GetArrayItem(temperature, 0)->valuedouble;
                limits.temperature.max = cJSON_GetArrayItem(temperature, 1)->valuedouble;
            }

            cJSON* humidity = cJSON_GetObjectItem(data_limits, "humidity");
            if (humidity) {
                limits.humidity.min = cJSON_GetArrayItem(humidity, 0)->valuedouble;
                limits.humidity.max = cJSON_GetArrayItem(humidity, 1)->valuedouble;
            }

            cJSON* pressure = cJSON_GetObjectItem(data_limits, "pressure");
            if (pressure) {
                limits.pressure.min = cJSON_GetArrayItem(pressure, 0)->valuedouble;
                limits.pressure.max = cJSON_GetArrayItem(pressure, 1)->valuedouble;
            }

            cJSON* white = cJSON_GetObjectItem(data_limits, "white");
            if (white) {
                limits.white.min = cJSON_GetArrayItem(white, 0)->valuedouble;
                limits.white.max = cJSON_GetArrayItem(white, 1)->valuedouble;
            }

            cJSON* visible = cJSON_GetObjectItem(data_limits, "visible");
            if (visible) {
                limits.visible.min = cJSON_GetArrayItem(visible, 0)->valuedouble;
                limits.visible.max = cJSON_GetArrayItem(visible, 1)->valuedouble;
            }

            cJSON* updateAvailable = cJSON_GetObjectItem(root, "updateAvailable");
            if (updateAvailable) {
                if (cJSON_IsTrue(updateAvailable)) {
                    ESP_LOGI(TAG, "Update available");
                    getUpdate();
                } else {
                    ESP_LOGI(TAG, "No update available, going to sleep");
                }
            }

            cJSON_Delete(root);
            ESP_ERROR_CHECK_WITHOUT_ABORT(save_limits(&limits));
            break;

        default:
            break;
    }
    return ESP_OK;
}

// Function to send data to the server
void send_data(double* moisture, double* temperature, double* humidity, double* pressure, double* white, double* visible, char* version) {
    char* data = malloc(150);
    sprintf(data, "{\"moisture\":%.2f,\"temperature\":%.2f,\"humidity\":%.2f,\"pressure\":%.2f,\"white\":%.5f,\"visible\":%.5f}",
            *moisture, *temperature, *humidity, *pressure, *white, *visible);
    ESP_LOGI(TAG, "Sending data: %s", data);

    esp_http_client_config_t config = {
        .url = URL,
        .method = HTTP_METHOD_POST,
        .cert_pem = NULL,
        .event_handler = http_client_event_handler};

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Version", version);
    esp_http_client_set_post_field(client, data, strlen(data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lli",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    free(data);
}

// Function to get an update
void getUpdate(void) {
    esp_http_client_config_t config = {
        .url = FIRMWARE_URL,
        .event_handler = http_client_event_handler,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA OK, restarting...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA failed...");
    }
}