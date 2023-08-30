#include "wifi.h"

#include <string.h>

#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
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

#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1
#define WIFI_CREDS_RECEIVED_BIT BIT2
#define MAXIMUM_RETRY           5

#define URL          BASE_URL "/api/measurements"
#define FIRMWARE_URL BASE_URL "/api/firmwareupdate"

#define BUFFSIZE 1024

static const char*        TAG = "WiFi";
static EventGroupHandle_t s_wifi_event_group;
static int                s_retry_num = 0;

char SSID[32] = "";
char PASSWORD[64] = "";

// Forward declarations
void getUpdate(void);

httpd_handle_t server = NULL;

esp_err_t hello_get_handler(httpd_req_t* req) {
    const char* page =
        ""
        "<!DOCTYPE html>"
        "<html lang=\"de\">"
        "<head>"
        "  <meta charset=\"UTF-8\">"
        "  <title>SSID und Passwort</title>"
        "  <style>"
        "    body {"
        "      font-family: Arial, sans-serif;"
        "      display: flex;"
        "      flex-direction: column;"
        "      align-items: center;"
        "      justify-content: center;"
        "      height: 100vh;"
        "      margin: 0;"
        "    }"
        "    input[type=\"text\"], input[type=\"password\"], button {"
        "      margin-bottom: 10px;"
        "      padding: 10px;"
        "      width: 90%;"
        "      max-width: 400px;"
        "    }"
        "  </style>"
        "</head>"
        "<body>"
        "  <input type=\"text\" id=\"ssid\" placeholder=\"SSID\" />"
        "  <input type=\"password\" id=\"password\" placeholder=\"Passwort\" />"
        "  <button onclick=\"sendData()\">Senden</button>"
        "  "
        "  <script>"
        "    function sendData() {"
        "      const ssid = document.getElementById(\"ssid\").value;"
        "      const password = document.getElementById(\"password\").value;"
        ""
        "      fetch(\"/creds\", {"
        "        method: \"POST\","
        "        headers: {"
        "          \"Content-Type\": \"application/json\""
        "        },"
        "        body: JSON.stringify({ ssid, password })"
        "      }).then(response => {"
        "        if (response.ok) {"
        "          alert(\"Daten gesendet. Die Einrichtung kann einige Sekunden dauern\");"
        "        } else {"
        "          alert(\"Fehler beim Senden der Daten.\");"
        "        }"
        "      });"
        "    }"
        "  </script>"
        "</body>"
        "</html>";

    httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t creds_post_handler(httpd_req_t* req) {
    char buffer[100];
    int  ret = httpd_req_recv(req, buffer, req->content_len);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    buffer[ret] = '\0';
    cJSON* json = cJSON_Parse(buffer);
    if (json == NULL) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Error before: %s", error_ptr);
        }
        return ESP_FAIL;
    }

    cJSON* ssid = cJSON_GetObjectItemCaseSensitive(json, "ssid");
    cJSON* password = cJSON_GetObjectItemCaseSensitive(json, "password");

    if (cJSON_IsString(ssid) && (ssid->valuestring != NULL) &&
        cJSON_IsString(password) && (password->valuestring != NULL)) {
        ESP_LOGI(TAG, "Received ssid: %s, password: %s", ssid->valuestring, password->valuestring);
    } else {
        ESP_LOGE(TAG, "JSON is not as expected");
        return ESP_FAIL;
    }

    strcpy(SSID, ssid->valuestring);
    strcpy(PASSWORD, password->valuestring);
    cJSON_Delete(json);

    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_RECEIVED_BIT);
    return ESP_OK;
}

void init_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_uri_t    hello = {
           .uri = "/",
           .method = HTTP_GET,
           .handler = hello_get_handler,
    };

    httpd_uri_t creds = {
        .uri = "/creds",
        .method = HTTP_POST,
        .handler = creds_post_handler,
    };

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &creds);
    }
}

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

void wifi_init_ap() {
    ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "TibSense",
            .max_connection = 1,
            .authmode = WIFI_AUTH_OPEN},
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
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
    wifi_config_t wifi_config;
    ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &wifi_config));
    ESP_LOGI(TAG, "Stored SSID: %s", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta finished. Wait for connection");
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    // Check which Bit is set
    if (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP");
    } else if (xEventGroupGetBits(s_wifi_event_group) & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to AP");
        xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
        esp_wifi_deinit();
        wifi_init_ap();
        init_webserver();
        xEventGroupWaitBits(s_wifi_event_group, WIFI_CREDS_RECEIVED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        httpd_stop(server);
        esp_wifi_stop();

        wifi_config_t wifi_config = {
            .sta = {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
        };
        strcpy((char*)wifi_config.sta.ssid, SSID);
        strcpy((char*)wifi_config.sta.password, PASSWORD);

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "wifi_init_sta finished. Wait for connection");
        xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    }
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