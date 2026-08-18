/*
đổi trạng thái đèn nháy bằng button qua ISR + Queue
 - 2 trạng thái:
   + NORMAL: đèn xanh nháy
   + ALERT : đèn đỏ nháy nhanh
trên esp32c3 - Door Gate Control Gate Module
*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
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


static QueueHandle_t myQueue;

static const char *TAG = "Queue ISR";

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
    int value = 1;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xQueueSendFromISR(
        myQueue,
        &value,
        &xHigherPriorityTaskWoken
    );

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
void myTask(void *pvParameters){
    int value;

    uint8_t current_state = NORMAL;

    gpio_num_t led = LED_BLUE_GPIO;
    uint8_t led_state = LED_OFF;

    uint64_t isr_time;

    while(1){
        if(current_state == NORMAL){
            if(xQueueReceive(myQueue, &value, pdMS_TO_TICKS(500)) == pdTRUE){
                ESP_LOGI(TAG, "ALERT MODE");

                gpio_set_level(led, LED_OFF);
                current_state = ALERT;
                isr_time = esp_timer_get_time();

                led_state = LED_ON;
                led = LED_RED_GPIO;

            }
            else{
                gpio_set_level(led, led_state);
                led_state = !led_state;
            }
            
        }
        else if(current_state == ALERT){
            if(xQueueReceive(myQueue, &value, pdMS_TO_TICKS(200)) == pdTRUE){
                ESP_LOGI(TAG, "RESET ALERT MODE");
                isr_time = esp_timer_get_time();
            }
            else{
                if(esp_timer_get_time() - isr_time > 4000000){
                    current_state = NORMAL;
                    led = LED_BLUE_GPIO;
                    led_state = LED_ON;
                    
                    gpio_set_level(LED_RED_GPIO, LED_OFF);
                }
                else{
                    gpio_set_level(led, led_state);
                    led_state = !led_state;
                }
            }
        }
    }
}

void app_main(void){
    myQueue = xQueueCreate(10,sizeof(int));

    // Khởi taoj
    gpio_init();

    gpio_install_isr_service(0);
    gpio_isr_handler_add(
        BUTTON_GPIO,
        button_isr_handler,
        (void *)BUTTON_GPIO
    );
    xTaskCreate(
        myTask,
        "my Task",
        2048,
        NULL,
        5,
        NULL
    );

 
}
