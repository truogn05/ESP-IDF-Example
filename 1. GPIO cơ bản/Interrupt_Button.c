/*
đổi trạng thái đèn xanh và đỏ với chu kì 1s
thêm ngắt ngoài BT1: mỗi lần nhấn -> tạm dừng / tiếp tục blink
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
#define BUTTON_GPIO     GPIO_NUM_10

// LED là Dương chung
#define LED_ON          0
#define LED_OFF         1
#define BLINK_PERIOD_MS 1000

typedef struct {
    uint8_t red;
    uint8_t blue;
} led_state_t;

static const char *TAG = "BUTTON ISR";
static volatile uint32_t last_button_isr_tick = 0;
static TaskHandle_t blink_task_handle;

/* Theo thứ tự bit Đỏ-Xanh: 11 -> 10 -> 01 -> 00 */
static const led_state_t led_states[] = {
    {LED_OFF, LED_OFF},
    {LED_OFF, LED_ON},
    {LED_ON, LED_OFF},
    {LED_ON, LED_ON},
};

static void set_led_state(size_t state_index)
{
    gpio_set_level(LED_RED_GPIO, led_states[state_index].red);
    gpio_set_level(LED_BLUE_GPIO, led_states[state_index].blue);

    ESP_LOGI(TAG, "RED: %s, BLUE: %s",
             led_states[state_index].red == LED_OFF ? "OFF" : "ON",
             led_states[state_index].blue) == LED_OFF ? "OFF" : "ON";
}

static void IRAM_ATTR button_isr_handler(void *arg)
{
    uint32_t now = xTaskGetTickCountFromISR();

    if ((now - last_button_isr_tick) > pdMS_TO_TICKS(100)) {
        BaseType_t higher_priority_task_woken = pdFALSE;

        last_button_isr_tick = now;
        vTaskNotifyGiveFromISR(blink_task_handle, &higher_priority_task_woken);

        if (higher_priority_task_woken) {
            portYIELD_FROM_ISR();
        }
    }
}

void app_main(void)
{
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_BLUE_GPIO) | (1ULL << LED_RED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);

    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&button_conf);

    blink_task_handle = xTaskGetCurrentTaskHandle();
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL));

    ESP_LOGI(TAG, "START");

    size_t state_index = 0;
    TickType_t last_change_tick = xTaskGetTickCount();
    bool blink_paused = false;

    set_led_state(state_index);

    while (1) {
        uint32_t button_presses = ulTaskNotifyTake(pdTRUE, 0);

        if (button_presses > 0) {
            blink_paused = !blink_paused;
            last_change_tick = xTaskGetTickCount();
            ESP_LOGI(TAG, "%s", blink_paused ? "Tạm dừng" : "Tiếp tục");
        }

        if (!blink_paused && (xTaskGetTickCount() - last_change_tick >= pdMS_TO_TICKS(BLINK_PERIOD_MS))) {
            state_index = (state_index + 1) % (sizeof(led_states) / sizeof(led_states[0]));
            set_led_state(state_index);
            last_change_tick = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
