/*
đổi trạng thái đèn xanh và đỏ sử dụng 2 timer riêng biệt
trên esp32c3 - Door Gate Control Gate Module
*/
#include <stdio.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// Định nghĩa chân GPIO
#define LED_RED_GPIO       GPIO_NUM_6
#define LED_BLUE_GPIO     GPIO_NUM_7

// LED là Dương chung
#define LED_ON          0
#define LED_OFF         1



void gpio_init(void){
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_BLUE_GPIO) | (1ULL << LED_RED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);
    gpio_set_level(LED_RED_GPIO, LED_OFF);
    gpio_set_level(LED_BLUE_GPIO, LED_OFF);
}

// Callback function khi timer kích hoạt
static void red_timer(void* arg) {
    
    static uint8_t red_state = LED_ON;

    ESP_LOGI("RED TIMER", "%s", red_state ? "OFF" : "ON");
    // Đổi trạng thái hiện tại của đèn
    gpio_set_level(LED_RED_GPIO, red_state);

    // Chuyển sang trạng thái kế tiếp
    red_state = !red_state;
}
static void blue_timer(void* arg) {
    
    static uint8_t blue_state = LED_ON;

    ESP_LOGI("BLUE TIMER", "%s", blue_state ? "OFF" : "ON");
    // Đổi trạng thái hiện tại của đèn
    gpio_set_level(LED_BLUE_GPIO, blue_state);
    
    // Chuyển sang trạng thái kế tiếp
    blue_state = !blue_state;
}

void app_main(void) {
    // Khởi tạo 
    gpio_init();

    const esp_timer_create_args_t red_timer_args = {
        .callback = &red_timer,
        .name = "RED Timer"
    };
    const esp_timer_create_args_t blue_timer_args = {
        .callback = &blue_timer,
        .name = "BLUE Timer"
    };


    esp_timer_handle_t red_timer_handler, blue_timer_handler;

    // Tạo timer
    esp_timer_create(&red_timer_args, &red_timer_handler);
    esp_timer_create(&blue_timer_args, &blue_timer_handler);

    // Bắt đầu timer lặp lại mỗi  1 giây 
    ESP_LOGI("Main Task: ", "Bắt đầu RED timer 1s");
    esp_timer_start_periodic(red_timer_handler, 1000000);

    ESP_LOGI("Main Task: ", "Bắt đầu BLUE timer 2s");
    esp_timer_start_periodic(blue_timer_handler, 2000000);  


    while(1){
        ESP_LOGI("MAIN", "Block");
        vTaskDelay(pdMS_TO_TICKS(3000));
        
    }
    
}