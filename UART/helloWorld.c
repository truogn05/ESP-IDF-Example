/*
gửi - nhận dữ liệu cơ bản qua UART
bằng esp32c3 - Door Gate Control Gate Module
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <string.h> 
#include <stdio.h>

// UART GPIO
#define UART_PORT       UART_NUM_0
#define UART_TX_GPIO    GPIO_NUM_21
#define UART_RX_GPIO    GPIO_NUM_20

#define UART_BAUD_RATE  115200
#define UART_BUF_SIZE   1024

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

void app_main(void){

    uart_init();

    char *msg = "Hello World!\r\n";
    uart_write_bytes(UART_PORT, msg, strlen(msg));  

    while(1){

        uart_write_bytes(UART_PORT, msg, strlen(msg));  

        uint8_t data[128];

        int len = uart_read_bytes(
            UART_PORT,
            data,
            sizeof(data) - 1,
            pdMS_TO_TICKS(100)
        );

        if (len > 0)
        {
            data[len] = '\0';

            printf("Received: %s\n", data);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }

}