/*
Kết nối wifi - mode Access Point
ESP32 đóng vai trò là điểm phát wifi
Gửi gói tin UDP qua điện thoại để điều khiển LED 
trên esp32c3 - Door Gate Control Gate Module
*/
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

// Cấu hình LED
#define LED_RED_GPIO            GPIO_NUM_6
#define LED_BLUE_GPIO           GPIO_NUM_7

#define LED_ON                  0   
#define LED_OFF                 1   

typedef struct {
    uint8_t red;
    uint8_t blue;
} led_state_t;
typedef struct {
    uint8_t state;
    struct sockaddr_in client_addr;
} led_cmd_t;

/* Theo thứ tự bit Đỏ-Xanh: 11 -> 10 -> 01 -> 00 */
static const led_state_t led_states[] = {
    {LED_OFF, LED_OFF},
    {LED_OFF, LED_ON},
    {LED_ON, LED_OFF},
    {LED_ON, LED_ON},
};
#define NUM_STATES (sizeof(led_states) / sizeof(led_states[0]))
// Cấu hình WIFI AP
#define WIFI_SSID      "ESP32C3"
#define WIFI_PASSWORD      "12345678"
#define WIFI_CHANNEL   1
#define MAX_STA_CONNECT       4

// UDP
#define UDP_PORT 3333
static const char *TAG = "WIFI AP";
static QueueHandle_t led_Queue = NULL;

static int sock = -1;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Có thiết bị đã kết nối | MAC: "MACSTR"", MAC2STR(event->mac));

    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGW(TAG, "Có Thiết bị đã ngắt kết nối | MAC: "MACSTR"", MAC2STR(event->mac));

    } else if (event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "Khởi động WIFI AP thành công");
    }
}


void wifi_init_softap(void)
{
    // Khởi tạo
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    // Cấu hình phát software AP
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASSWORD,
            .max_connection = MAX_STA_CONNECT,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif
            .pmf_cfg = {
                .required = true,
            },
        },
    };

    if (strlen(WIFI_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    
    // Bắt đầu phát Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());

    // In thông tin 
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(ap_netif, &ip_info);
    ESP_LOGI(TAG, "Khởi tạo ESP32 AP thành công");
    ESP_LOGI(TAG, "SSID     : %s", WIFI_SSID);
    ESP_LOGI(TAG, "Password : %s", WIFI_PASSWORD);
    ESP_LOGI(TAG, "IP ESP   : " IPSTR, IP2STR(&ip_info.ip));
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
    led_cmd_t cmd;
    const char *tag = pcTaskGetName(NULL);
    while(1){
        xQueueReceive(led_Queue, &cmd, portMAX_DELAY);
        uint8_t state = cmd.state;
        struct sockaddr_in client = cmd.client_addr;
        char payload[128];
        snprintf(payload, sizeof(payload), "Đổi trạng thái thành công: RED: %s, BLUE: %s", led_states[state].red == LED_OFF ? "OFF" : "ON", led_states[state].blue == LED_OFF ? "OFF" : "ON");
        
        ESP_LOGI(tag, "Đổi sang Trạng thái: %u", state);
        gpio_set_level(LED_RED_GPIO, led_states[state].red);
        gpio_set_level(LED_BLUE_GPIO, led_states[state].blue);
        int sent = sendto(
            sock,
            payload,
            strlen(payload),
            0,
            (struct sockaddr *)&client,
            sizeof(client)

        );
        if(sent < 0){
            ESP_LOGE(tag, "K gửi được dữ liệu");
        }
        else{
            ESP_LOGI(tag, "Đã gửi thành công %d bytes đến client [%s:%d]", sent, inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        //vTaskDelay(pdMS_TO_TICKS(20));
    }
}



void udp_server(void *pvParameters){
    char receive_buffer[10];
    struct sockaddr_in server_addr, client_addr;
    const char *tag = pcTaskGetName(NULL);

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if(sock < 0){
        ESP_LOGE(tag, "Lỗi khi tạo Server");
        vTaskDelete(NULL);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(UDP_PORT);

    int err = bind( sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (err < 0)
    {
        ESP_LOGE(tag, "Có lỗi xảy ra");
        close(sock);
        vTaskDelete(NULL);
    }

    ESP_LOGI(tag, "UDP Server port %d", UDP_PORT);

    while(1){
        socklen_t socklen = sizeof(client_addr);
        
        int len = recvfrom(sock, receive_buffer, sizeof(receive_buffer)-1, 0, (struct sockaddr *)&client_addr, &socklen);

        if(len<0){
            ESP_LOGE(tag, "Xảy ra lỗi");
            break;
        }

        receive_buffer[len] = '\0';
        ESP_LOGI(tag, "Received: [%s]",receive_buffer);
        uint8_t state = receive_buffer[0] - '0';
        
        if(state>3) {
            char * payload = "Chỉ có thể gửi từ 0-3";
            int sent = sendto(sock, payload, strlen(payload), 0, (struct sockaddr*)&client_addr,socklen );
            if(sent < 0){
                ESP_LOGE(tag, "K gửi được dữ liệu");
            }
            else{
                ESP_LOGI(tag, "Đã gửi thành công %d bytes đến client [%s:%d]", sent, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            }
            continue;
        }
        led_cmd_t cmd = {state, client_addr};
        xQueueSend(led_Queue, &cmd, 0);

        
    }
    close(sock);
    vTaskDelete(NULL);
}
void app_main(void)
{
    // Khởi tạo NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Bắt đầu chế độ Wi-Fi AP...");
    wifi_init_softap();

    gpio_init();

    led_Queue = xQueueCreate(10, sizeof(led_cmd_t));

    xTaskCreate(
        udp_server,
        "UDP Server",
        2048,
        NULL,
        1,
        NULL
    );
    xTaskCreate(
        led_task,
        "LED Task",
        2048,
        NULL,
        1,
        NULL
    );

}
