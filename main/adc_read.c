#include "adc_read.h"
#include "esp_adc/adc_continuous.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// ESP32-S3 Specific: ADC1 Channel 6 is GPIO 7
#define PRESSURE_CHANNEL ADC_CHANNEL_6   

static adc_continuous_handle_t adc_handle = NULL;
QueueHandle_t pressureQueue = NULL; 

void adc_init(void) {
    pressureQueue = xQueueCreate(10, sizeof(uint16_t));

    adc_continuous_handle_cfg_t handle_cfg = {
        .max_store_buf_size = 1024,
        .conv_frame_size = 256,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&handle_cfg, &adc_handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 2000,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };

    adc_digi_pattern_config_t pattern = {
        .atten = ADC_ATTEN_DB_12, 
        .channel = PRESSURE_CHANNEL,
        .unit = ADC_UNIT_1,
        .bit_width = ADC_BITWIDTH_12,
    };
    dig_cfg.pattern_num = 1;
    dig_cfg.adc_pattern = &pattern;

    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
}

void adc_task(void *pvParameters) {
    uint8_t result[256] = {0};
    uint32_t ret_num = 0;

    while (1) {
        esp_err_t ret = adc_continuous_read(adc_handle, result, sizeof(result), &ret_num, 0);

        if (ret == ESP_OK && ret_num > 0) {
            uint32_t sum = 0; 
            int count = 0;

            for (int i = 0; i < ret_num; i += sizeof(adc_digi_output_data_t)) {
                adc_digi_output_data_t *out = (adc_digi_output_data_t*)&result[i];
                if (out->type2.unit == ADC_UNIT_1 && out->type2.channel == PRESSURE_CHANNEL) {
                    sum += out->type2.data;
                    count++;
                }
            }

            if (count > 0 && pressureQueue != NULL) {
                uint16_t avg_val = sum / count;
                xQueueOverwrite(pressureQueue, &avg_val);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}