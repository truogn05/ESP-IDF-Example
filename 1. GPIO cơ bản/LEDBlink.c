/*
đổi trạng thái đèn xanh và đỏ với chu kì 2s
trên esp32c3 - Door Gate Control Gate Module
*/
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

// Định nghĩa chân GPIO 
#define LED_RED_GPIO    GPIO_NUM_6
#define LED_BLUE_GPIO   GPIO_NUM_7

// LED là Dương chung
#define LED_ON          0
#define LED_OFF         1

static const char *TAG = "LED_BLINK";

static void gpio_init(void){
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_BLUE_GPIO) | (1ULL << LED_RED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    gpio_config(&led_conf);

    gpio_set_level(LED_RED_GPIO, LED_OFF);
    gpio_set_level(LED_BLUE_GPIO, LED_OFF);
}

void app_main(void){

    gpio_init();

    ESP_LOGI(TAG, "Bắt đầu");
    while(1){
        
        // 1. Xanh: ON     Đỏ: OFF
        ESP_LOGI(TAG, " 1. Xanh: ON     Đỏ: OFF");
        gpio_set_level(LED_BLUE_GPIO, LED_ON);
        gpio_set_level(LED_RED_GPIO, LED_OFF);
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 2. Xanh: ON     Đỏ: ON
        ESP_LOGI(TAG, " 2. Xanh: ON     Đỏ: ON");
        gpio_set_level(LED_BLUE_GPIO, LED_ON);
        gpio_set_level(LED_RED_GPIO, LED_ON);
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 3. Xanh: OFF    Đỏ: ON
        ESP_LOGI(TAG, " 3. Xanh: OFF    Đỏ: ON");
        gpio_set_level(LED_BLUE_GPIO, LED_OFF);
        gpio_set_level(LED_RED_GPIO, LED_ON);
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 4. Xanh: OFF    Đỏ: OFF
        ESP_LOGI(TAG, " 4. Xanh: OFF    Đỏ: OFF");
        gpio_set_level(LED_BLUE_GPIO, LED_OFF);
        gpio_set_level(LED_RED_GPIO, LED_OFF);
        vTaskDelay(pdMS_TO_TICKS(1000));

    }

}