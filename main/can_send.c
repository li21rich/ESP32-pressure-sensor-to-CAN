#include "can_send.h"
#include <driver/twai.h>
#include <driver/gpio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "CAN_TASK";
#define CAN_TX_PIN      GPIO_NUM_14   // CAN TX
#define CAN_RX_PIN      GPIO_NUM_13   // CAN RX
#define CAN_BAUD_RATE   TWAI_TIMING_CONFIG_500KBITS()

extern QueueHandle_t pressureQueue;

void can_init(void) {
    // Configure TWAI 
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    g_config.tx_queue_len = 10;
    g_config.rx_queue_len = 5;
    
    twai_timing_config_t t_config = CAN_BAUD_RATE;
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install Driver
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        ESP_LOGI(TAG, "Driver installed");
    } else {
        ESP_LOGE(TAG, "Failed to install driver");
        return;
    }

    // Start driver
    if (twai_start() == ESP_OK) {
        ESP_LOGI(TAG, "Driver started");
    } else {
        ESP_LOGE(TAG, "Failed to start driver");
        return;
    }

    // Enable alerts
    uint32_t alerts_to_enable = TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_RECOVERED | TWAI_ALERT_TX_FAILED;
    twai_reconfigure_alerts(alerts_to_enable, NULL);
}

void can_task(void *pvParameters) {
    uint16_t raw_pressure_val;
    uint32_t alerts;

    while (1) {
        // Bus recovery
        if (twai_read_alerts(&alerts, 0) == ESP_OK) {
            if (alerts & TWAI_ALERT_BUS_OFF) {
                ESP_LOGE(TAG, "Bus-Off detected! Initiating Recovery...");
                twai_initiate_recovery(); 
            }
            if (alerts & TWAI_ALERT_BUS_RECOVERED) {
                ESP_LOGI(TAG, "Bus Recovered. Restarting...");
                twai_start();
            }
        }

        // CAN send
        if (xQueueReceive(pressureQueue, &raw_pressure_val, pdMS_TO_TICKS(100)) == pdTRUE) {
            
            twai_message_t msg = {
                .identifier = 0xC1, // ID 193 (Log_Driver_Inputs)
                .extd = 0,
                .data_length_code = 8,
                .flags = TWAI_MSG_FLAG_NONE
            };

            // DBC Packing: Brake_Pedal_Pressure_Front (0.1 scale)
            // Raw 12-bit ADC -> Bytes 0 and 1
            msg.data[0] = (uint8_t)(raw_pressure_val & 0xFF);
            msg.data[1] = (uint8_t)((raw_pressure_val >> 8) & 0xFF);
            
            // Clear remaining bytes
            for(int i=2; i<8; i++) msg.data[i] = 0;

            // Transmit
            esp_err_t res = twai_transmit(&msg, pdMS_TO_TICKS(10));
            
            if (res != ESP_OK) {
                // Might comment it out.
                ESP_LOGW(TAG, "Tx Failed: %s", esp_err_to_name(res));
            }
        }
    }
}