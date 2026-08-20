/*
đổi trạng thái đèn xanh và đỏ với chu kì 1s sử dụng timer 
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

static const char *TAG = "ESP TIMER";
//static const char *TAG = "TASK: ";

typedef struct {
    uint8_t red;
    uint8_t blue;
} led_state_t;

static const led_state_t led_states[] = {
    {LED_OFF, LED_OFF},
    {LED_OFF, LED_ON},
    {LED_ON, LED_OFF},
    {LED_ON, LED_ON},
};
#define NUM_STATES (sizeof(led_states) / sizeof(led_states[0]))


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
static void periodic_timer_callback(void* arg) {
    
    static uint8_t state_idx = 0;
    // Đổi trạng thái hiện tại của đèn
    gpio_set_level(LED_RED_GPIO, led_states[state_idx].red);
    gpio_set_level(LED_BLUE_GPIO, led_states[state_idx].blue);
    ESP_LOGI(TAG, "State [%d] -> RED: %s, BLUE: %s", 
                state_idx,
                led_states[state_idx].red == LED_ON ? "ON" : "OFF",
                led_states[state_idx].blue == LED_ON ? "ON" : "OFF");

    // Chuyển sang trạng thái kế tiếp
    state_idx = (state_idx + 1) % NUM_STATES;
}


void app_main(void) {
    // Khởi tạo 
    gpio_init();

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &periodic_timer_callback,
        .name = "periodic_timer"
    };

    esp_timer_handle_t periodic_timer;
    // Tạo timer
    esp_timer_create(&periodic_timer_args, &periodic_timer);

    // Bắt đầu timer lặp lại mỗi  1 giây 
    ESP_LOGI("Main Task: ", "Bắt đầu Timer 1s");
    esp_timer_start_periodic(periodic_timer, 1000000);

    while(1){
        ESP_LOGI("MAIN", "Block");
        vTaskDelay(pdMS_TO_TICKS(2000));
        
    }
    
}