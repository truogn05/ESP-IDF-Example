/*
đổi trạng thái đèn 
sử dụng semaphore couting đếm ISR mà k mất sự kiện như Semaphore Binary
trên esp32c3 - Door Gate Control Gate Module
*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
// Định nghĩa chân GPIO
#define LED_RED_GPIO       GPIO_NUM_6
#define LED_BLUE_GPIO      GPIO_NUM_7
#define BUTTON_GPIO        GPIO_NUM_10

// LED là Dương chung
#define LED_ON          0
#define LED_OFF         1

#define NORMAL  1
#define ALERT   0


// Khai báo con trỏ Semaphore
static SemaphoreHandle_t mySemaphore = NULL;

static const char *TAG = "semaphore Couting";

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
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&button_conf);

    gpio_set_level(LED_RED_GPIO, LED_OFF);
    gpio_set_level(LED_BLUE_GPIO, LED_OFF);
}

void IRAM_ATTR button_isr_handler(void *arg)
{
    static int64_t last_press = 0;
    int64_t now = esp_timer_get_time();

    if(now - last_press < 200000) return;

    last_press = now;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xSemaphoreGiveFromISR(mySemaphore, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        //chuyển ngữ cảnh ngay
        portYIELD_FROM_ISR();
    }
}
void buttonTask(void *pvParameters){
    uint32_t counter = 0;

    while(1){
        if(xSemaphoreTake(mySemaphore, portMAX_DELAY) == pdTRUE){
            counter++;
            
            UBaseType_t remaining = uxSemaphoreGetCount(mySemaphore);
            ESP_LOGW(TAG, "===> [BẮT ĐẦU] Xử lý công việc #%lu | Còn lại: %u việc", counter, (unsigned int)remaining);

            gpio_set_level(LED_RED_GPIO, LED_ON);

            vTaskDelay(pdMS_TO_TICKS(1500));

            gpio_set_level(LED_RED_GPIO, LED_OFF);

            ESP_LOGI(TAG, "===> [Hoàn thành]");
        }
    }
}

void app_main(void){
    // Khởi tạo
    gpio_init();

    mySemaphore = xSemaphoreCreateCounting(5,0);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(
        BUTTON_GPIO,
        button_isr_handler,
        (void *)BUTTON_GPIO
    );

    xTaskCreate(
        buttonTask,
        "Button Task",
        2048,
        NULL,
        5,
        NULL
    );
}
