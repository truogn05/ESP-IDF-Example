/*
Kết nối wifi và kết nối tới Mqtt broker (local)
gửi trạng thái hiện tại các led 
và nhận dữ liệu từ MQTT để điều khiển LED
trên esp32c3 - Door Gate Control Gate Module
*/

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "driver/gpio.h"

// Cấu hình LED
#define LED_RED_GPIO            GPIO_NUM_6
#define LED_BLUE_GPIO           GPIO_NUM_7

#define LED_ON                  0   
#define LED_OFF                 1   

// Cấu hình WIFI
#define WIFI_SSID          "RD"      
#define WIFI_PASS          "Vconnex@1102"           
#define MAXIMUM_RETRY      10

// Cấu hình MQTT
#define BROKER_URI "mqtt://192.168.1.21:1883"
#define TOPIC_SUB  "/test/sub"
#define TOPIC_PUB  "/test/pub"

static const char *TAG = "MQTT";
static int retry_num = 0;

static QueueHandle_t myQueue = NULL;
static esp_mqtt_client_handle_t mqtt_client = NULL;
typedef struct {
    uint8_t red;
    uint8_t blue;
} led_state_t;

/* Theo thứ tự bit Đỏ-Xanh: 11 -> 10 -> 01 -> 00 */
static const led_state_t led_states[] = {
    {LED_OFF, LED_OFF},
    {LED_OFF, LED_ON},
    {LED_ON, LED_OFF},
    {LED_ON, LED_ON},
};
#define NUM_STATES (sizeof(led_states) / sizeof(led_states[0]))

static void mqtt_init(void);

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Bắt đầu kết nối tới wifi...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGW(TAG, "Lỗi, Thử kết nối lại ... (Lần %d/%d)", retry_num, MAXIMUM_RETRY);
        } else {
            ESP_LOGE(TAG, "Kết nối WiFi thất bại!");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        retry_num = 0;
        
        ESP_LOGI(TAG, "KẾT NỐI WIFI THÀNH CÔNG!");
        ESP_LOGI(TAG, "Địa chỉ IP: " IPSTR, IP2STR(&event->ip_info.ip));
        
        ESP_LOGI(TAG, "Bắt đầu khởi tạo MQTT...");
        mqtt_init();
    }
}

static void wifi_init_sta(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Đang khởi động WiFi với SSID: [%s]", WIFI_SSID);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data){
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch( event_id){
        case MQTT_EVENT_CONNECTED:

            ESP_LOGI(TAG, "MQTT kết nối thành công");
            mqtt_client = client;
            msg_id = esp_mqtt_client_subscribe(client, TOPIC_SUB, 0);
            ESP_LOGI(TAG, "Subscribe topic %s thành công, msg_id=%d", TOPIC_SUB, msg_id);

            esp_mqtt_client_publish(client, TOPIC_PUB, "ESP32 kết nối thành công", 0, 1, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT mất kết nối, đang tự động kết nối lại...");
            mqtt_client = NULL;
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Nhận được dữ liệu: %.*s", event->data_len, event->data);
            if(myQueue != NULL){
                char rcv;
                if(event->data_len == 1){
                    rcv = event->data[0]-'0';
                    if( rcv >3) {
                        ESP_LOGW(TAG, "Dữ liệu không hợp lệ");
                        break;
                    }
                    xQueueSend(myQueue, &rcv, 0);
                }
                
            }
            break;
        default:
            break;
    }
    return ;
}

static void mqtt_init(void){
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = BROKER_URI,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}
static void gpio_init(void){
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_BLUE_GPIO) | (1ULL << LED_RED_GPIO),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    gpio_config(&led_conf);

    gpio_set_level(LED_RED_GPIO, LED_OFF);
    gpio_set_level(LED_BLUE_GPIO, LED_OFF);
}

void led_task(void *pvParameters){
    uint8_t state;
    while(1){
        xQueueReceive(myQueue, &state, portMAX_DELAY);
        ESP_LOGI(pcTaskGetName(NULL), ": Trạng thái hiện tại: %u", state);
        gpio_set_level(LED_RED_GPIO, led_states[state].red);
        gpio_set_level(LED_BLUE_GPIO, led_states[state].blue);

        //vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void pub_task(void *pvParameters){
    while(1){
        vTaskDelay(pdMS_TO_TICKS(5000));
        char payload[64];
        uint8_t red, blue;
        red = gpio_get_level(LED_RED_GPIO);
        blue = gpio_get_level(LED_BLUE_GPIO);
        if(mqtt_client !=NULL){
            snprintf(payload, sizeof(payload), "Current state: RED: %s, BLUE: %s", red == LED_OFF ? "OFF" : "ON", blue == LED_OFF ? "OFF" : "ON");
            esp_mqtt_client_publish(mqtt_client, TOPIC_PUB, payload, 0, 1, 0);

            ESP_LOGI(pcTaskGetName(NULL), ": Gửi trạng thái hiện tại thành công");
        }

    }
}

void app_main(void){
    gpio_init();

    myQueue = xQueueCreate(10, sizeof(uint8_t));

    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Khởi động chương trình kết nối WiFi...");

    esp_err_t ret = nvs_flash_init();

    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "Bắt đầu kết nối WiFi...");
    wifi_init_sta();
    xTaskCreate(
        led_task,
        "Led Task",
        2048,
        NULL,
        5, 
        NULL
    );

    xTaskCreate(
        pub_task, 
        "Publish Task",
        2048,
        NULL, 
        5,
        NULL
    );
}