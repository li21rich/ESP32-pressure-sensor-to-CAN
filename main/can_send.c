/**
 * Author: Richard Li
 * Editors: Richard Li
 */

#include <driver/twai.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

extern QueueHandle_t pressureQueue;

void can_init(void) {
    // General config — normal mode, pins must match board
    twai_general_config_t g_config = {
        .mode = TWAI_MODE_NORMAL,
        .tx_io = GPIO_NUM_2,    // <-- TODO change to right TX pin
        .rx_io = GPIO_NUM_1,    // <-- TODO change to right RX pin
        .clkout_io = TWAI_IO_UNUSED,
        .bus_off_io = TWAI_IO_UNUSED,
        .tx_queue_len = 5,
        .rx_queue_len = 5,
        .alerts_enabled = TWAI_ALERT_NONE,
        .clkout_divider = 0,
    };

    // Standard 500 kbps timing (change if needed)
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    twai_driver_install(&g_config, &t_config, &f_config);
    twai_start();
}

void can_task(void *pvParameters) {
    uint16_t pressure_val;

    while (1) {
        BaseType_t got = xQueueReceive(pressureQueue, &pressure_val, pdMS_TO_TICKS(500));

        printf("[CAN Task] Queue=%u, %s pressure=%u\n",
            (unsigned int)uxQueueMessagesWaiting(pressureQueue),
            got == pdTRUE ? "Sent" : "No new",
            got == pdTRUE ? pressure_val : 0
        );

        if (got == pdTRUE) {
            twai_message_t msg = {
                .identifier = 0x100,
                .data_length_code = 2,
                .data = {
                    (uint8_t)(pressure_val & 0xFF),
                    (uint8_t)((pressure_val >> 8) & 0xFF)
                },
                .flags = TWAI_MSG_FLAG_NONE
            };
            twai_transmit(&msg, pdMS_TO_TICKS(10)); 
        }
    }
}
