/*
đổi trạng thái đèn xanh và đỏ sử dụng PWM
sử dụng hàm ledc_set_fade_with_time() có sẵn thay vì dùng vòng lặp
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

// Định nghĩa chân GPIO
#define LED_RED_GPIO       GPIO_NUM_6
#define LED_BLUE_GPIO     GPIO_NUM_7

//Cấu hình 
#define LEDC_MODE          LEDC_LOW_SPEED_MODE
#define LEDC_TIMER         LEDC_TIMER_0
#define LEDC_RESOLUTION    LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY_HZ  5000
#define LEDC_MAX_DUTY      ((1U << LEDC_RESOLUTION) - 1U)

#define FADE_TIME_MS       2000U

static const char *TAG = "LED PWM";

// target_brightness: 0 = tat, LEDC_MAX_DUTY = Sáng tối đa. 
static void fade_led_brightness(ledc_channel_t channel, uint32_t target_brightness, uint32_t fade_time_ms, ledc_fade_mode_t wait_mode)
{
    if (target_brightness > LEDC_MAX_DUTY) {
        target_brightness = LEDC_MAX_DUTY;
    }

    // Led dương chung -> pwm = max - brightness
    uint32_t target_duty = LEDC_MAX_DUTY - target_brightness;

    ESP_ERROR_CHECK(ledc_set_fade_with_time(LEDC_MODE, channel, target_duty, fade_time_ms));
    ESP_ERROR_CHECK(ledc_fade_start(LEDC_MODE, channel, wait_mode));
}

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
        .duty = LEDC_MAX_DUTY, //Xanh tắt
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&red_channel));
    ESP_ERROR_CHECK(ledc_channel_config(&blue_channel));

    // Khởi tạo dịch vụ LEDC Fade Driver
    ESP_ERROR_CHECK(ledc_fade_func_install(0));

    ESP_LOGI(TAG, "START");
    while (true) {
        // Đỏ tăng
        ESP_LOGI(TAG, "RED INCREASE");
        fade_led_brightness(LEDC_CHANNEL_0, LEDC_MAX_DUTY, FADE_TIME_MS, LEDC_FADE_WAIT_DONE);

        // Đỏ giảm 
        ESP_LOGI(TAG, "RED DECREASE");
        fade_led_brightness(LEDC_CHANNEL_0, 0, FADE_TIME_MS, LEDC_FADE_WAIT_DONE);

        // Xanh tăng
        ESP_LOGI(TAG, "BLUE INCREASE");
        fade_led_brightness(LEDC_CHANNEL_1, LEDC_MAX_DUTY, FADE_TIME_MS, LEDC_FADE_WAIT_DONE);

        // Xanh giảm 
        ESP_LOGI(TAG, "BLUE DECREASE");
        fade_led_brightness(LEDC_CHANNEL_1, 0, FADE_TIME_MS, LEDC_FADE_WAIT_DONE);

        // Cả 2 tăng
        ESP_LOGI(TAG, "RED + BLUE INCREASE");
        fade_led_brightness(LEDC_CHANNEL_0, LEDC_MAX_DUTY, FADE_TIME_MS, LEDC_FADE_NO_WAIT);
        fade_led_brightness(LEDC_CHANNEL_1, LEDC_MAX_DUTY, FADE_TIME_MS, LEDC_FADE_WAIT_DONE);
        
        // Cả 2 giảm 
        ESP_LOGI(TAG, "RED + BLUE DECREASE");
        fade_led_brightness(LEDC_CHANNEL_0, 0, FADE_TIME_MS, LEDC_FADE_NO_WAIT);
        fade_led_brightness(LEDC_CHANNEL_1, 0, FADE_TIME_MS, LEDC_FADE_WAIT_DONE);
    }
}
