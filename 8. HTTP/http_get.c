/*
Lấy thời gian hiện tại định kì qua public API
trên esp32c3 - Door Gate Control Gate Module
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#define WIFI_SSID   "RD"
#define WIFI_PASS   "Vconnex@1102"

static const char *TAG = "TIME API";
static char response_buffer[1024];
static int response_len = 0;

static esp_err_t http_event_handler(esp_http_client_event_t *evt){
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (response_len + evt->data_len < sizeof(response_buffer)) {
            memcpy(response_buffer + response_len, evt->data, evt->data_len);
            response_len += evt->data_len;
            response_buffer[response_len] = '\0';
        }
    }
    return ESP_OK;
}

static void get_json_str(const char *json, const char *key, char *out, int max_len) {
    char search_key[32];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    char *p = strstr(json, search_key);
    if (p) {
        p += strlen(search_key);
        while (*p && (*p == ':' || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
            p++;
        }
        if (*p == '"') {
            p++;
            char *end = strchr(p, '"');
            if (end) {
                int len = end - p;
                if (len >= max_len) len = max_len - 1;
                strncpy(out, p, len);
                out[len] = '\0';
                return;
            }
        }
    }
    strcpy(out, "N/A");
}

static int get_json_int(const char *json, const char *key) {
    char search_key[32];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    char *p = strstr(json, search_key);
    if (p) {
        p += strlen(search_key);
        while (*p && (*p == ':' || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
            p++;
        }
        return atoi(p);
    }
    return 0;
}

static void parse_and_print_time(const char *json_str){
    char  time_str[16], dayOfWeek[16];

    get_json_str(json_str, "time", time_str, sizeof(time_str));
    get_json_str(json_str, "dayOfWeek", dayOfWeek, sizeof(dayOfWeek));
    int seconds = get_json_int(json_str, "seconds");
    int day = get_json_int(json_str, "day");
    int month = get_json_int(json_str, "month");
    int year = get_json_int(json_str, "year");
    ESP_LOGI(TAG, " [%s] DATE: %02d/%02d/%d | TIME: %s:%02d", dayOfWeek, day, month, year, time_str, seconds);
}

static void time_fetch_task(void *pvParameters){
    esp_http_client_config_t config = {
        .url = "https://timeapi.io/api/Time/current/zone?timeZone=Asia/Ho_Chi_Minh",
        .event_handler = http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 8000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    while (1) {
        response_len = 0;
        memset(response_buffer, 0, sizeof(response_buffer));

        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            parse_and_print_time(response_buffer);
        } else {
            ESP_LOGE(TAG, "HTTP GET loi: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data){
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Ket noi lai wifi...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Da nhan IP: " IPSTR, IP2STR(&event->ip_info.ip));
        
        static bool task_created = false;
        if (!task_created) {
            xTaskCreate(time_fetch_task, "time_fetch_task", 6144, NULL, 5, NULL);
            task_created = true;
        }
    }
}

void wifi_init_sta(void){
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

void app_main(void){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();
}