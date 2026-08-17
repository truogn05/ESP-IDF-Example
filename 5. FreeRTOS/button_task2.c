/*
đổi trạng thái đèn xanh và đỏ 
hiệu ứng fade
bấm nút sẽ thay đổi hiệu ứng fade của led
trên esp32c3 - Door Gate Control Gate Module
*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

// Định nghĩa chân GPIO
#define LED_RED_GPIO       GPIO_NUM_6
#define LED_BLUE_GPIO      GPIO_NUM_7
#define BUTTON_GPIO        GPIO_NUM_10

// LED là Dương chung
#define LED_ON          0
#define LED_OFF         1

// Cấu hình LEDC PWM
#define LEDC_MODE          LEDC_LOW_SPEED_MODE
#define LEDC_TIMER         LEDC_TIMER_0
#define LEDC_RESOLUTION    LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY_HZ  5000
#define LEDC_MAX_DUTY      ((1U << LEDC_RESOLUTION) - 1U)

// Thời gian 1 pha fade 
#define FADE_PHASE_MS      1000U

// Kênh LEDC
#define RED_CHANNEL        LEDC_CHANNEL_0
#define BLUE_CHANNEL       LEDC_CHANNEL_1
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

static const char *TAG = "LED BLINK";
static volatile uint8_t idx_state = 0;

static void gpio_init(void){

    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&button_conf);
}

void ledc_init(void){
    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_RESOLUTION,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_config);

    const ledc_channel_config_t red_channel = {
        .gpio_num = LED_RED_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = RED_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = LEDC_MAX_DUTY, // Đỏ tắt 
        .hpoint = 0,
    };
    const ledc_channel_config_t blue_channel = {
        .gpio_num = LED_BLUE_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = BLUE_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = LEDC_MAX_DUTY, // Xanh tắt
        .hpoint = 0,
    };
    ledc_channel_config(&red_channel);
    ledc_channel_config(&blue_channel);

    // Khởi tạo dịch vụ LEDC 
    ledc_fade_func_install(0);
}

// Polling đọc button
void button_task(void *pvParameters){
    
    static TickType_t last_pressed = 0;
    uint8_t was_pressed = 0;
    while(1){
        uint8_t is_presssed = (gpio_get_level(BUTTON_GPIO) == 0);
        TickType_t now = xTaskGetTickCount();

        if(is_presssed && (!was_pressed &&(now - last_pressed >= pdMS_TO_TICKS(100)))){
            idx_state = (idx_state+1)%NUM_STATES;
            last_pressed = now;
        }
        was_pressed = is_presssed;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
// Đổi trạng thái led
void led_ctrl(void *pvParameters){
    
    while(1){
        switch (idx_state)
        {
        case 0:
            ledc_stop(LEDC_MODE, RED_CHANNEL, LEDC_MAX_DUTY);
            ledc_stop(LEDC_MODE, BLUE_CHANNEL, LEDC_MAX_DUTY);
            break;
        case 1:
            ledc_stop(LEDC_MODE, BLUE_CHANNEL, LEDC_MAX_DUTY);
            ledc_set_fade_time_and_start(LEDC_MODE, RED_CHANNEL, 0, 500, LEDC_FADE_WAIT_DONE);
            ledc_set_fade_time_and_start(LEDC_MODE, RED_CHANNEL, LEDC_MAX_DUTY, 500, LEDC_FADE_WAIT_DONE);
            break;
        case 2:
            ledc_stop(LEDC_MODE, RED_CHANNEL, LEDC_MAX_DUTY);
            ledc_set_fade_time_and_start(LEDC_MODE, BLUE_CHANNEL, 0, 500, LEDC_FADE_WAIT_DONE);
            ledc_set_fade_time_and_start(LEDC_MODE, BLUE_CHANNEL, LEDC_MAX_DUTY, 500, LEDC_FADE_WAIT_DONE);
            break;
        case 3:
            ledc_set_fade_time_and_start(LEDC_MODE, RED_CHANNEL, 0, 500, LEDC_FADE_NO_WAIT);
            ledc_set_fade_time_and_start(LEDC_MODE, BLUE_CHANNEL, 0, 500, LEDC_FADE_WAIT_DONE);
            ledc_set_fade_time_and_start(LEDC_MODE, RED_CHANNEL, LEDC_MAX_DUTY, 500, LEDC_FADE_NO_WAIT);
            ledc_set_fade_time_and_start(LEDC_MODE, BLUE_CHANNEL, LEDC_MAX_DUTY, 500, LEDC_FADE_WAIT_DONE);
            break;
        default:
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
void app_main(void){
    // Khởi taoj
    gpio_init();
    ledc_init();

    xTaskCreate(
        button_task,
        "button Task",
        2048,
        NULL,
        5,
        NULL
    );

    xTaskCreate(
        led_ctrl,
        "LED Task",
        2048,
        NULL,
        3,
        NULL
    );
}