/*
Nhận lệnh điều khiển LED qua UART
 - 0 : Tắt cả 2 LED
 - 1 : Chỉ fade RED
 - 2 : Chỉ fade BLUE
 - 3 : Fade cả 2 LED
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

// Định nghĩa chân GPIO LED
#define LED_RED_GPIO       GPIO_NUM_6
#define LED_BLUE_GPIO      GPIO_NUM_7

// UART GPIO
#define UART_PORT          UART_NUM_0
#define UART_TX_GPIO       GPIO_NUM_21
#define UART_RX_GPIO       GPIO_NUM_20

#define UART_BAUD_RATE     115200
#define UART_BUF_SIZE      1024

// Cấu hình LEDC PWM
#define LEDC_MODE          LEDC_LOW_SPEED_MODE
#define LEDC_TIMER         LEDC_TIMER_0
#define LEDC_RESOLUTION    LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY_HZ  5000
#define LEDC_MAX_DUTY      ((1U << LEDC_RESOLUTION) - 1U)

// Thời gian 1 pha fade 
#define FADE_PHASE_MS      1000U

// Kênh PWM
#define RED_CHANNEL        LEDC_CHANNEL_0
#define BLUE_CHANNEL       LEDC_CHANNEL_1

static const char *TAG = "LED FADE";

// Biến toàn cục lưu Mode hiện tại 
static volatile uint8_t g_mode = 0;

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
        UART_PIN_NO_CHANGE
    );

    uart_driver_install(
        UART_PORT,
        UART_BUF_SIZE * 2,
        UART_BUF_SIZE * 2,
        0,
        NULL,
        0
    );
}

void ledc_init(void)
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
        .channel = RED_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = LEDC_MAX_DUTY, // Đỏ tắt 
        .hpoint = 0,
    };
    const ledc_channel_config_t blue_channel = {
        .gpio_num = LED_BLUE_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = BLUE_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = LEDC_MAX_DUTY, // Xanh tắt
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&red_channel));
    ESP_ERROR_CHECK(ledc_channel_config(&blue_channel));

    // Khởi tạo dịch vụ LEDC Fade Driver
    ESP_ERROR_CHECK(ledc_fade_func_install(0));
}

// điều khiển fade led
static void fade_ctrl(ledc_channel_t channel, bool enable_fade, bool fade_in, uint32_t fade_time_ms)
{
    if (!enable_fade) {
        // Dừng fade và tắt LED 
        ledc_stop(LEDC_MODE, channel, LEDC_MAX_DUTY);
        return;
    }

    // LED Dương chung -> Max = LOW, Min = HIGH
    uint32_t target_brightness = fade_in ? LEDC_MAX_DUTY : 0;
    uint32_t target_duty = LEDC_MAX_DUTY - target_brightness;

    ledc_set_fade_with_time(LEDC_MODE, channel, target_duty, fade_time_ms);
    ledc_fade_start(LEDC_MODE, channel, LEDC_FADE_NO_WAIT);
}

static void led_fade_task(void *arg)
{
    bool fade_in = true;
    uint8_t last_mode = 255;
    uint32_t step_counter = 0;
    const uint32_t STEP_TICKS_MS = 50; // Kiểm tra trạng thái mode mỗi 50ms
    const uint32_t TOTAL_STEPS = FADE_PHASE_MS / STEP_TICKS_MS; // 1000ms / 50ms = 20 bước

    while (1) {
        uint8_t current_mode = g_mode;

        // Nếu người dùng đổi mode mới HOẶC Hết 1s
        if (current_mode != last_mode || step_counter >= TOTAL_STEPS) {
            if (current_mode != last_mode) {
                last_mode = current_mode;
                fade_in = true; // Bắt đầu bằng pha tăng độ sáng khi đổi mode mới
                ESP_LOGI(TAG, "Chuyển sang Mode %d", current_mode);
            } else {
                fade_in = !fade_in; // Đảo chiều fade (Tăng <-> Giảm)
            }
            step_counter = 0;

            // Xác định LED nào được bật fade theo Mode
            bool red_enable  = (current_mode == 1 || current_mode == 3);
            bool blue_enable = (current_mode == 2 || current_mode == 3);

            // Gọi hàm fade_ctrl không gây block
            fade_ctrl(RED_CHANNEL, red_enable, fade_in, FADE_PHASE_MS);
            fade_ctrl(BLUE_CHANNEL, blue_enable, fade_in, FADE_PHASE_MS);
        }

        vTaskDelay(pdMS_TO_TICKS(STEP_TICKS_MS));
        step_counter++;
    }
}

void app_main(void)
{
    uart_init();
    ledc_init();

    ESP_LOGI(TAG, "START");

    // Tạo Task chạy ngầm xử lý Fade LED
    xTaskCreate(led_fade_task, "led_fade_task", 4096, NULL, 5, NULL);

    uint8_t data[64];

    // Luồng chính 
    while (1) {
        int len = uart_read_bytes(
            UART_PORT,
            data,
            sizeof(data) - 1,
            pdMS_TO_TICKS(50)
        );

        if (len > 0) {
            data[len] = '\0';
            // Lọc qua toàn bộ chuỗi nhận 
            for (int i = 0; i < len; i++) {
                if (data[i] >= '0' && data[i] <= '3') {
                    uint8_t new_mode = data[i] - '0';
                    g_mode = new_mode;
                    ESP_LOGI(TAG, "Received: %d", new_mode);
                }
            }
        }
    }
}