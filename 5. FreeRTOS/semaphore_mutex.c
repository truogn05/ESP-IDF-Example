/*
đổi trạng thái đèn nháy
sử dụng semaphore cmutex
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
static SemaphoreHandle_t mySemMutex = NULL;
static uint32_t shared_data = 0;

static const char *TAG = "MUTEX: ";

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

void RED_task(void *pvParameters){
    int8_t red = LED_OFF;

    while(1){
        
        
        if(xSemaphoreTake(mySemMutex, pdMS_TO_TICKS(1000)) == pdTRUE){

            shared_data +=10;
            ESP_LOGI(TAG, "RED TASK -> START");
            ESP_LOGI(TAG, "RED TASK -> counter = %lu", shared_data);
            ESP_LOGI(TAG, "RED TASK -> END");

            red = !red;
            gpio_set_level(LED_RED_GPIO, red);
            xSemaphoreGive(mySemMutex);
        }
        else{
            ESP_LOGW(TAG, "RED TASK -> Timeout!");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void BLUE_task(void *pvParameters){
    int8_t blue = LED_OFF;

    while(1){
        
        
        if(xSemaphoreTake(mySemMutex, pdMS_TO_TICKS(1000)) == pdTRUE){

            shared_data -=5;
            ESP_LOGI(TAG, "BLUE TASK -> START");
            ESP_LOGI(TAG, "BLUE TASK -> counter = %lu", shared_data);
            ESP_LOGI(TAG, "BLUE TASK -> END");
            blue = !blue;
            gpio_set_level(LED_BLUE_GPIO, blue);
            xSemaphoreGive(mySemMutex);
        }
        else{
            ESP_LOGW(TAG, "BLUE TASK -> Timeout!");
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


void app_main(void){
    // Khởi taoj
    gpio_init();

    mySemMutex = xSemaphoreCreateMutex();

    xTaskCreate(
        RED_task,
        "Red Task",
        2048,
        NULL,
        1,
        NULL
    );

    xTaskCreate(
        BLUE_task,
        "Blue Task",
        2048,
        NULL,
        1,
        NULL
    );
}