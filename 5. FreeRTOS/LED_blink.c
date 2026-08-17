/*
đổi trạng thái đèn xanh và đỏ 
với 2 task riêng biệt sử dụng freeRTOS
 - task 1: đỏ đổi màu với chu kì 1s
 - task 2: xanh đổi màu với chu kì 2s
trên esp32c3 - Door Gate Control Gate Module
*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

// Định nghĩa chân GPIO
#define LED_RED_GPIO       GPIO_NUM_6
#define LED_BLUE_GPIO     GPIO_NUM_7

// LED là Dương chung
#define LED_ON          0
#define LED_OFF         1


static const char *TAG = "FreeRTOS";

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

void RED_task(void *pvParameters){
    int8_t red = LED_OFF;

    while(1){
        ESP_LOGI("RED: ", "%s", (red == LED_OFF) ? "OFF" : "ON");
        gpio_set_level(LED_RED_GPIO, red);
        red = !red;
        vTaskDelay(pdMS_TO_TICKS(500));

    }
}

void BLUE_task(void *pvParameters){
    int8_t blue = LED_OFF;

    while(1){
        ESP_LOGI("BLUE: ", "%s", (blue == LED_OFF) ? "OFF" : "ON");
        gpio_set_level(LED_BLUE_GPIO, blue);
        blue = !blue;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void){
    // Khởi taoj
    gpio_init();

    xTaskCreate(
        RED_task,
        "Red Task",
        2048,
        NULL,
        1,
        NULL
    );

    xTaskCreate(
        BLUE_task,
        "Blue Task",
        2048,
        NULL,
        1,
        NULL
    );
}