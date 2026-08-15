/*
đổi trạng thái đèn xanh và đỏ với user button BT1
nhấn - đổi trạng thái 1 lần
nhấn giữ - đổi trạng thái với chu kì 1s
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
#define BUTTON          GPIO_NUM_10

// LED là Dương chung
#define LED_ON          0
#define LED_OFF         1

static const char *TAG = "LED_BLINK";

typedef struct {
    uint8_t red;
    uint8_t blue;
} led_state_t;

/* Theo thứ tự bit Đỏ-Xanh: 11 -> 10 -> 01 -> 00 */
static const led_state_t led_states[] = {
    {LED_OFF, LED_OFF},
    {LED_OFF, LED_ON},
    {LED_ON, LED_OFF},
    {LED_ON,  LED_ON},
};

static void set_led_state(size_t state_index)
{
    gpio_set_level(LED_RED_GPIO, led_states[state_index].red);
    gpio_set_level(LED_BLUE_GPIO, led_states[state_index].blue);

    ESP_LOGI(TAG, "Trang thai %u: Do-Xanh = %u%u",
             (unsigned)(state_index + 1),
             led_states[state_index].red,
             led_states[state_index].blue);
}

void app_main(void)
{

    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_RED_GPIO) | (1ULL << LED_BLUE_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);

    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&button_conf);

    ESP_LOGI(TAG, "--- Bat dau ---");
    size_t state_index = 0;
    bool was_pressed = false;
    TickType_t last_change_tick = 0;

    set_led_state(state_index);

    while (1) {
        const bool is_pressed = (gpio_get_level(BUTTON) == 0);
        const TickType_t now = xTaskGetTickCount();

        // Nhấn 1 lần or Giữ 1s sẽ đổi trạng thái
        if (is_pressed && (!was_pressed || (now - last_change_tick >= pdMS_TO_TICKS(1000)))) {
            state_index = (state_index + 1) % (sizeof(led_states) / sizeof(led_states[0]));
            set_led_state(state_index);
            last_change_tick = now;
        }

        was_pressed = is_pressed;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
