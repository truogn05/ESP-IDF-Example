/*
Kết nối wifi - mode Access Point
ESP32 đóng vai trò là điểm phát wifi
trên esp32c3 - Door Gate Control Gate Module
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

// Cấu hình WIFI AP
#define WIFI_SSID      "ESP32C3"
#define WIFI_PASSWORD      "12345678"
#define WIFI_CHANNEL   1
#define MAX_STA_CONNECT       4

static const char *TAG = "WIFI AP";

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

    // In thông tin IP mặc định của ESP32
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(ap_netif, &ip_info);
    ESP_LOGI(TAG, "Khởi tạo ESP32 AP thành công");
    ESP_LOGI(TAG, "SSID     : %s", WIFI_SSID);
    ESP_LOGI(TAG, "Password : %s", WIFI_PASSWORD);
    ESP_LOGI(TAG, "IP ESP   : " IPSTR, IP2STR(&ip_info.ip));
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
}
