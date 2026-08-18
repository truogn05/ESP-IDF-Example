/*
đổi trạng thái đèn led = Web server sử dụng HTTP
trên esp32c3 - Door Gate Control Gate Module
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
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

typedef enum {
    MODE_OFF = 0,
    MODE_ON,
    MODE_BLINK
} led_mode_t;

static const char *TAG = "WIFI HTTP";
static led_mode_t red_mode = MODE_OFF;
static led_mode_t blue_mode = MODE_OFF;
static uint8_t red_blink_state = LED_OFF;
static uint8_t blue_blink_state = LED_OFF;
static httpd_handle_t server = NULL;

static void blink_timer_cb(void *arg){
    if (red_mode == MODE_BLINK) {
        red_blink_state = (red_blink_state == LED_ON) ? LED_OFF : LED_ON;
        gpio_set_level(LED_RED_GPIO, red_blink_state);
    }
    if (blue_mode == MODE_BLINK) {
        blue_blink_state = (blue_blink_state == LED_ON) ? LED_OFF : LED_ON;
        gpio_set_level(LED_BLUE_GPIO, blue_blink_state);
    }
}

static void timer_init(void){
    const esp_timer_create_args_t timer_args = {
        .callback = &blink_timer_cb,
        .name = "blink timer"
    };
    esp_timer_handle_t timer_handle;
    esp_timer_create(&timer_args, &timer_handle);
    esp_timer_start_periodic(timer_handle, 500000);
}

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

static const char html_page[] = 
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>ESP32 Web Server</title>"
        "<style>"
        "body{font-family:Arial,sans-serif;padding:20px;margin:0}"
        ".row{margin:15px 0;display:flex;align-items:center;gap:10px}"
        ".led{width:16px;height:16px;border-radius:50%;background:#ccc;display:inline-block}"
        ".led.red{background:red}"
        ".led.blue{background:blue}"
        "button{padding:6px 12px;cursor:pointer;background:#eee;border:1px solid #ccc;border-radius:4px}"
        "button:active{background:#999;color:#fff}"
        "</style></head><body>"
        "<h3>ESP32 Web Server</h3>"
        "<div class='row'>"
        "<span>LED RED:</span>"
        "<button onclick=\"setLed('red','on')\">ON</button>"
        "<button onclick=\"setLed('red','off')\">OFF</button>"
        "<button onclick=\"setLed('red','blink')\">BLINK</button>"
        "</div>"
        "<div class='row'>"
        "<span>LED BLUE:</span>"
        "<button onclick=\"setLed('blue','on')\">ON</button>"
        "<button onclick=\"setLed('blue','off')\">OFF</button>"
        "<button onclick=\"setLed('blue','blink')\">BLINK</button>"
        "</div>"
        "<script>"
        "function updateUI(d){"
        "document.getElementById('red-dot').className='led '+(d.red==='off'?'':'red');"
        "document.getElementById('blue-dot').className='led '+(d.blue==='off'?'':'blue');"
        "}"
        "function setLed(c,a){fetch('/api/'+c+'/'+a).then(r=>r.json()).then(updateUI);}"
        "fetch('/api/status').then(r=>r.json()).then(updateUI);"
        "</script></body></html>";

static void send_status_json(httpd_req_t *req){
    char resp[64];
    const char *r_str = (red_mode == MODE_ON) ? "on" : (red_mode == MODE_BLINK) ? "blink" : "off";
    const char *b_str = (blue_mode == MODE_ON) ? "on" : (blue_mode == MODE_BLINK) ? "blink" : "off";
    snprintf(resp, sizeof(resp), "{\"red\":\"%s\",\"blue\":\"%s\"}", r_str, b_str);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t root_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req){
    send_status_json(req);
    return ESP_OK;
}

static esp_err_t red_cmd_handler(httpd_req_t *req){
    if (strstr(req->uri, "/on")) {
        red_mode = MODE_ON;
        gpio_set_level(LED_RED_GPIO, LED_ON);
    } else if (strstr(req->uri, "/off")) {
        red_mode = MODE_OFF;
        gpio_set_level(LED_RED_GPIO, LED_OFF);
    } else if (strstr(req->uri, "/blink")) {
        red_mode = MODE_BLINK;
    }
    send_status_json(req);
    return ESP_OK;
}

static esp_err_t blue_cmd_handler(httpd_req_t *req){
    if (strstr(req->uri, "/on")) {
        blue_mode = MODE_ON;
        gpio_set_level(LED_BLUE_GPIO, LED_ON);
    } else if (strstr(req->uri, "/off")) {
        blue_mode = MODE_OFF;
        gpio_set_level(LED_BLUE_GPIO, LED_OFF);
    } else if (strstr(req->uri, "/blink")) {
        blue_mode = MODE_BLINK;
    }
    send_status_json(req);
    return ESP_OK;
}

void start_webserver(void){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_root = {.uri = "/", .method = HTTP_GET, .handler = root_handler};
        httpd_uri_t uri_status = {.uri = "/api/status", .method = HTTP_GET, .handler = status_handler};
        
        httpd_uri_t uri_r_on = {.uri = "/api/red/on", .method = HTTP_GET, .handler = red_cmd_handler};
        httpd_uri_t uri_r_off = {.uri = "/api/red/off", .method = HTTP_GET, .handler = red_cmd_handler};
        httpd_uri_t uri_r_blink = {.uri = "/api/red/blink", .method = HTTP_GET, .handler = red_cmd_handler};

        httpd_uri_t uri_b_on = {.uri = "/api/blue/on", .method = HTTP_GET, .handler = blue_cmd_handler};
        httpd_uri_t uri_b_off = {.uri = "/api/blue/off", .method = HTTP_GET, .handler = blue_cmd_handler};
        httpd_uri_t uri_b_blink = {.uri = "/api/blue/blink", .method = HTTP_GET, .handler = blue_cmd_handler};

        httpd_register_uri_handler(server, &uri_root);
        httpd_register_uri_handler(server, &uri_status);
        httpd_register_uri_handler(server, &uri_r_on);
        httpd_register_uri_handler(server, &uri_r_off);
        httpd_register_uri_handler(server, &uri_r_blink);
        httpd_register_uri_handler(server, &uri_b_on);
        httpd_register_uri_handler(server, &uri_b_off);
        httpd_register_uri_handler(server, &uri_b_blink);

        ESP_LOGI(TAG, "Webserver started");
    }
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
    timer_init();
    wifi_init_sta();
}
