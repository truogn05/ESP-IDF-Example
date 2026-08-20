/*
đổi trạng thái đèn xanh và đỏ sử dụng PWM
trên esp32c3 - Door Gate Control Gate Module
*/
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include <math.h> 

// Định nghĩa chân GPIO
#define LED_RED_GPIO       GPIO_NUM_6
#define LED_BLUE_GPIO     GPIO_NUM_7

//Cấu hình 
#define LEDC_MODE          LEDC_LOW_SPEED_MODE
#define LEDC_TIMER         LEDC_TIMER_0
#define LEDC_RESOLUTION    LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY_HZ  5000
#define LEDC_MAX_DUTY      ((1U << LEDC_RESOLUTION) - 1U)

// Kênh LEDC
#define RED_CHANNEL        LEDC_CHANNEL_0
#define BLUE_CHANNEL       LEDC_CHANNEL_1


static const char *TAG = "LED PWM";

void app_main(void)
{
    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_RESOLUTION,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    const ledc_channel_config_t red_channel = {
        .gpio_num = LED_RED_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = LEDC_MAX_DUTY, //Đỏ tắt
        .hpoint = 0,
    };
    const ledc_channel_config_t blue_channel = {
        .gpio_num = LED_BLUE_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0, //Xanh sáng
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&red_channel));
    ESP_ERROR_CHECK(ledc_channel_config(&blue_channel));

    ledc_fade_func_install(0);
    
    ESP_LOGI(TAG, "START");
    while (true) {
        // Đỏ tăng, Xanh giảm
        ESP_LOGI(TAG, "RED");
        ledc_set_fade_time_and_start(LEDC_MODE, RED_CHANNEL, 0, 2000, LEDC_FADE_NO_WAIT);
        ledc_set_fade_time_and_start(LEDC_MODE, BLUE_CHANNEL, LEDC_MAX_DUTY, 2000, LEDC_FADE_WAIT_DONE);
        // Đỏ giảm, Xanh tăng
        ESP_LOGI(TAG, "BLUE");
        ledc_set_fade_time_and_start(LEDC_MODE, BLUE_CHANNEL, 0, 2000, LEDC_FADE_NO_WAIT);
        ledc_set_fade_time_and_start(LEDC_MODE, RED_CHANNEL, LEDC_MAX_DUTY, 2000, LEDC_FADE_WAIT_DONE);
        //vTaskDelay(pdMS_TO_TICKS(1000));

    }
}
