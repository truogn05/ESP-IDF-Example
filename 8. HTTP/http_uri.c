/*
đổi trạng thái đèn led = uri
trên esp32c3 - Door Gate Control Gate Module
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"

#define LED_RED_GPIO       GPIO_NUM_6
#define LED_BLUE_GPIO      GPIO_NUM_7

#define LED_ON             0
#define LED_OFF            1

#define WIFI_SSID          "RD"
#define WIFI_PASS          "Vconnex@1102"

static const char *TAG = "WIFI_LED";
static uint8_t led_state = LED_OFF;
static httpd_handle_t server = NULL;

static void gpio_init(void){
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_RED_GPIO) | (1ULL << LED_BLUE_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);

    gpio_set_level(LED_RED_GPIO, LED_OFF);
    gpio_set_level(LED_BLUE_GPIO, LED_OFF);
}

static esp_err_t on_handler(httpd_req_t *req){
    led_state = LED_ON;
    gpio_set_level(LED_RED_GPIO, led_state);
    ESP_LOGI(TAG, "LED ON");
    const char *resp = "LED is ON\n";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t off_handler(httpd_req_t *req){
    led_state = LED_OFF;
    gpio_set_level(LED_RED_GPIO, led_state);
    ESP_LOGI(TAG, "LED OFF");
    const char *resp = "LED is OFF\n";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t toggle_handler(httpd_req_t *req){
    led_state = (led_state == LED_ON) ? LED_OFF : LED_ON;
    gpio_set_level(LED_RED_GPIO, led_state);
    ESP_LOGI(TAG, "LED TOGGLE -> %s", (led_state == LED_ON) ? "ON" : "OFF");
    char resp[32];
    snprintf(resp, sizeof(resp), "LED is %s\n", (led_state == LED_ON) ? "ON" : "OFF");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req){
    char resp[32];
    snprintf(resp, sizeof(resp), "LED Status: %s\n", (led_state == LED_ON) ? "ON" : "OFF");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t uri_on = {
    .uri       = "/on",
    .method    = HTTP_GET,
    .handler   = on_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_off = {
    .uri       = "/off",
    .method    = HTTP_GET,
    .handler   = off_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_toggle = {
    .uri       = "/toggle",
    .method    = HTTP_GET,
    .handler   = toggle_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_status = {
    .uri       = "/status",
    .method    = HTTP_GET,
    .handler   = status_handler,
    .user_ctx  = NULL
};

void start_webserver(void){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_on);
        httpd_register_uri_handler(server, &uri_off);
        httpd_register_uri_handler(server, &uri_toggle);
        httpd_register_uri_handler(server, &uri_status);
        ESP_LOGI(TAG, "Webserver started");
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data){
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Thử kết nối lại...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&event->ip_info.ip));
        if (server == NULL) {
            start_webserver();
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

    gpio_init();
    wifi_init_sta();
}
