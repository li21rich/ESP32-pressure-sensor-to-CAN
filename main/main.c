/**
* Author: Richard Li
* Editors: Richard Li
*/

#include <stdio.h>
#include "esp_adc/adc_continuous.h"
#include "freertos/freeRTOS.h"

#define PRESSURE_CHANNEL ADC_CHANNEL_6   // GPIO34

void app_main(void) {
    adc_continuous_handle_t adc_handle;
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

    uint8_t result[256];
    while (true) {
        uint32_t bytes_read = 0;
        esp_err_t ret = adc_continuous_read(adc_handle, result, sizeof(result), &bytes_read, 1000);
        if (ret == ESP_OK && bytes_read > 0) {
            for (int i = 0; i < bytes_read; i += sizeof(adc_digi_output_data_t)) {
                adc_digi_output_data_t *out = (adc_digi_output_data_t*)&result[i];
                printf("ADC val: %d\n", out->type2.data);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


// for CAN, //QueueHandle_t pressureQueue = xQueueCreate(10, sizeof(int));