#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

// Định nghĩa chân GPIO
#define LED_RED_GPIO       GPIO_NUM_6
#define LED_BLUE_GPIO      GPIO_NUM_7
#define BUTTON_GPIO        GPIO_NUM_10

// LED là Dương chung
#define LED_ON          0
#define LED_OFF         1

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

static QueueHandle_t myQueue;


static void gpio_init(void){
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_BLUE_GPIO) | (1ULL << LED_RED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    gpio_config(&led_conf);

    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&button_conf);

    gpio_set_level(LED_RED_GPIO, LED_OFF);
    gpio_set_level(LED_BLUE_GPIO, LED_OFF);
}

// Polling đọc button
void button_queue(void *pvParameters){
    int data = 1;
    static TickType_t last_pressed = 0;
    uint8_t was_pressed = 0;
    while(1){
        uint8_t is_presssed = (gpio_get_level(BUTTON_GPIO) == 0);
        TickType_t now = xTaskGetTickCount();

        if(is_presssed && (!was_pressed &&(now - last_pressed >= pdMS_TO_TICKS(100)))){
            xQueueSend(myQueue, &data, pdMS_TO_TICKS(200));
            last_pressed = now;
        }
        was_pressed = is_presssed;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Đổi trạng thái led
void led_ctrl(void *pvParameters){
    uint8_t idx = 0;
    while(1){
        int data = 0;
        xQueueReceive(myQueue, &data, 0);
        if(data){
            idx = (idx+1) % NUM_STATES;
        }
        gpio_set_level(LED_BLUE_GPIO, led_states[idx].blue);
        gpio_set_level(LED_RED_GPIO, led_states[idx].red);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_set_level(LED_BLUE_GPIO, LED_OFF);
        gpio_set_level(LED_RED_GPIO, LED_OFF);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


void app_main(void){
    myQueue = xQueueCreate(10,sizeof(int));

    // Khởi taoj
    gpio_init();

    xTaskCreate(
        button_queue,
        "Button Task",
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
