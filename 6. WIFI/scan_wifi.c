/*
Quét và in ra thông tin các mạng Wifi trong phạm vi
trên esp32c3 - Door Gate Control Gate Module
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#define DEFAULT_SCAN_LIST_SIZE 10

static const char *TAG = "WIFI SCAN ";

// Hàm chuyển đổi 
static const char* print_auth_mode(wifi_auth_mode_t authmode) {
    switch (authmode) {
        case WIFI_AUTH_OPEN:
            return "OPEN";
        case WIFI_AUTH_WEP:
            return "WEP";
        case WIFI_AUTH_WPA_PSK:
            return "WPA_PSK";
        case WIFI_AUTH_WPA2_PSK:
            return "WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA/WPA2_PSK";
        case WIFI_AUTH_ENTERPRISE:
            return "ENTERPRISE";
        case WIFI_AUTH_WPA3_PSK:
            return "WPA3_PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "WPA2/WPA3_PSK";
        case WIFI_AUTH_WAPI_PSK:
            return "WAPI_PSK";
        case WIFI_AUTH_OWE:
            return "OWE";
        default:
            return "UNKNOWN";
    }
}


void wifi_scan(void){
    //cấu hình
    wifi_scan_config_t scan_cf = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE
    };

    ESP_LOGI(TAG, "Bắt đầu quét wifi..." );
    esp_wifi_scan_start(&scan_cf, true);

    uint16_t ap_num;
    esp_wifi_scan_get_ap_num(&ap_num);
    ESP_LOGI(TAG, "Tổng số WIFI tìm thấy: %u", ap_num);
    
    uint16_t n = 20;
    wifi_ap_record_t list[n];
    esp_wifi_scan_get_ap_records(&n, list);

    ESP_LOGI(TAG, "%-4s | %-32s | %-4s | %-6s | %-12s", "STT", "SSID", "KÊNH", "RSSI", "BẢO MẬT");

    for (int i = 0; i < n; i++) {
        ESP_LOGI(TAG, "%-4d | %-32.32s | %-4u | %-6d | %-12s",
                 i + 1,
                 (strlen((char *)list[i].ssid) > 0) ? (char *)list[i].ssid : "Hidden",
                 list[i].primary,
                 list[i].rssi,
                 print_auth_mode(list[i].authmode));
    }
    
}

void app_main(void){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    wifi_scan();

}

