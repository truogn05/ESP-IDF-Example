/*
đổi trạng thái đèn xanh và đỏ sử dụng PWM
sử dụng biến đổi gamma cho mắt người
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

#define FADE_STEP          64U
#define FADE_STEP_MS       20U

static const char *TAG = "LED PWM";

// brightness: 0 = tat, LEDC_MAX_DUTY = Sáng tối đa. 
static void set_led_brightness(ledc_channel_t channel, uint32_t brightness)
{
    if(brightness > LEDC_MAX_DUTY){
        brightness = LEDC_MAX_DUTY;
    }

    float normalized = (float)brightness / (float)LEDC_MAX_DUTY;
    uint32_t gamma_duty = (uint32_t)(powf(normalized, 2.2f) * LEDC_MAX_DUTY + 0.5f);

    // Led dương chung -> pwm = max - brightness
    uint32_t duty = LEDC_MAX_DUTY - gamma_duty;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, channel, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, channel));
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
        .duty = 0, //Xanh sáng
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&red_channel));
    ESP_ERROR_CHECK(ledc_channel_config(&blue_channel));

    ESP_LOGI(TAG, "START");
    while (true) {
        // Đỏ tăng, Xanh giảm
        ESP_LOGI(TAG, "RED");
        for (uint32_t red = 0; red <= LEDC_MAX_DUTY; red += FADE_STEP) {
            set_led_brightness(LEDC_CHANNEL_0, red);
            set_led_brightness(LEDC_CHANNEL_1, LEDC_MAX_DUTY - red);
            vTaskDelay(pdMS_TO_TICKS(FADE_STEP_MS));
        }
        //vTaskDelay(pdMS_TO_TICKS(1000));
        set_led_brightness(LEDC_CHANNEL_0, 0);
        set_led_brightness(LEDC_CHANNEL_1, LEDC_MAX_DUTY);
        // Đỏ giảm, Xanh tăng
        ESP_LOGI(TAG, "BLUE");
        for (uint32_t red = LEDC_MAX_DUTY; red >= FADE_STEP; red -= FADE_STEP) {
            set_led_brightness(LEDC_CHANNEL_0, red);
            set_led_brightness(LEDC_CHANNEL_1, LEDC_MAX_DUTY - red);
            vTaskDelay(pdMS_TO_TICKS(FADE_STEP_MS));
        }
        set_led_brightness(LEDC_CHANNEL_0, LEDC_MAX_DUTY);
        set_led_brightness(LEDC_CHANNEL_1, 0);
        //vTaskDelay(pdMS_TO_TICKS(1000));

    }
}
