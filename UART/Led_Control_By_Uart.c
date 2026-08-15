/*
Nhận lệnh điều khiển LED qua UART
 - 0 : Tắt cả 2 LED
 - 1 : Chỉ bật RED
 - 2 : Chỉ bật BLUE
 - 3 : Bật cả 2 LED
bằng esp32c3 - Door Gate Control Gate Module
*/
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"

// Định nghĩa chân GPIO
#define LED_RED_GPIO       GPIO_NUM_6
#define LED_BLUE_GPIO     GPIO_NUM_7

// LED là Dương chung
#define LED_ON          0
#define LED_OFF         1

// UART GPIO
#define UART_PORT       UART_NUM_0
#define UART_TX_GPIO    GPIO_NUM_21
#define UART_RX_GPIO    GPIO_NUM_20

#define UART_BAUD_RATE  115200
#define UART_BUF_SIZE   1024

//Cấu hình 
#define LEDC_MODE          LEDC_LOW_SPEED_MODE
#define LEDC_TIMER         LEDC_TIMER_0
#define LEDC_RESOLUTION    LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY_HZ  5000
#define LEDC_MAX_DUTY      ((1U << LEDC_RESOLUTION) - 1U)

#define FADE_TIME_MS       2000U

//channel
#define RED_CHANNEL LEDC_CHANNEL_0
#define BLUE_CHANNEL LEDC_CHANNEL_1

//static const char *TAG = "TASK: ";

typedef struct {
    uint8_t red;
    uint8_t blue;
} led_state_t;

static const led_state_t led_states[] = {
    {LED_OFF, LED_OFF},
    {LED_OFF, LED_ON},
    {LED_ON, LED_OFF},
    {LED_ON, LED_ON},
};

void gpio_init(void){
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_BLUE_GPIO) | (1ULL << LED_RED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);
}

void uart_init(void)
{
    uart_config_t uart_cfg = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    uart_param_config(UART_PORT, &uart_cfg);
    uart_set_pin(
            UART_PORT,
            UART_TX_GPIO,
            UART_RX_GPIO,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE);

    uart_driver_install(
            UART_PORT,
            UART_BUF_SIZE * 2,
            UART_BUF_SIZE * 2,
            0,
            NULL,
            0);
}

void ledc_init(void){
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
        .channel = RED_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = LEDC_MAX_DUTY, //Đỏ tắt
        .hpoint = 0,
    };
    const ledc_channel_config_t blue_channel = {
        .gpio_num = LED_BLUE_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = BLUE_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = LEDC_MAX_DUTY, //Xanh tắt
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&red_channel));
    ESP_ERROR_CHECK(ledc_channel_config(&blue_channel));

    // Khởi tạo dịch vụ LEDC Fade Driver
    ESP_ERROR_CHECK(ledc_fade_func_install(0));
}

void led_ctrl(uint8_t mode){
    if (mode >= sizeof(led_states) / sizeof(led_states[0])) {
        return;
    }
    uint8_t red = led_states[mode].red;
    uint8_t blue = led_states[mode].blue;
    
    gpio_set_level(LED_RED_GPIO, red);
    gpio_set_level(LED_BLUE_GPIO, blue);
}
void app_main(void){

    gpio_init();
    uart_init();
    //ledc_init();
    
    uint8_t mode = 0;
    
    while(1){
        led_ctrl(mode);
        uint8_t data[16];

        int len = uart_read_bytes(
            UART_PORT,
            data,
            sizeof(data),
            pdMS_TO_TICKS(100)
        );

        if (len > 0)
        {
            uint8_t new_mode = data[0] - '0';
            if (new_mode < (sizeof(led_states) / sizeof(led_states[0]))) {
                mode = new_mode;
            }
        }
    }
}