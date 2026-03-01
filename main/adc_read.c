#include "adc_read.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/*
 A B
 C D
IC3:
 14 19
 6 4
IC4:
 8 10
 16 15
*/

#define NUM_SENSORS 8

typedef struct {
    adc_unit_t    unit;
    adc_channel_t channel;
    const char*   label;
} sensor_cfg_t;

static const sensor_cfg_t sensors[NUM_SENSORS] = {
    { ADC_UNIT_1, ADC_CHANNEL_5, "IC3_C(GPIO6)"  },
    { ADC_UNIT_1, ADC_CHANNEL_3, "IC3_D(GPIO4)"  },
    { ADC_UNIT_1, ADC_CHANNEL_7, "IC4_A(GPIO8)"  },
    { ADC_UNIT_1, ADC_CHANNEL_9, "IC4_B(GPIO10)" },
    { ADC_UNIT_2, ADC_CHANNEL_3, "IC3_A(GPIO14)" },
    { ADC_UNIT_2, ADC_CHANNEL_8, "IC3_B(GPIO19)" },
    { ADC_UNIT_2, ADC_CHANNEL_5, "IC4_C(GPIO16)" },
    { ADC_UNIT_2, ADC_CHANNEL_4, "IC4_D(GPIO15)" },
};

static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_oneshot_unit_handle_t adc2_handle = NULL;
QueueHandle_t pressureQueue = NULL;

static float to_mv(int raw) {
    return (raw / 4095.0f) * (1100.0f * 2.818f);
}

void adc_init(void) {
    pressureQueue = xQueueCreate(8, sizeof(uint16_t));

    // Init ADC1
    adc_oneshot_unit_init_cfg_t adc1_cfg = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc1_cfg, &adc1_handle));

    // Init ADC2
    adc_oneshot_unit_init_cfg_t adc2_cfg = { .unit_id = ADC_UNIT_2 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc2_cfg, &adc2_handle));

    // Configure all 8 channels
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    for (int i = 0; i < NUM_SENSORS; i++) {
        adc_oneshot_unit_handle_t h = (sensors[i].unit == ADC_UNIT_1) ? adc1_handle : adc2_handle;
        ESP_ERROR_CHECK(adc_oneshot_config_channel(h, sensors[i].channel, &chan_cfg));
    }
}

void adc_task(void *pvParameters) {
    while (1) {
        for (int i = 0; i < NUM_SENSORS; i++) {
            int raw = 0;
            adc_oneshot_unit_handle_t h = (sensors[i].unit == ADC_UNIT_1) ? adc1_handle : adc2_handle;
            ESP_ERROR_CHECK(adc_oneshot_read(h, sensors[i].channel, &raw));
            printf("%s | raw: %d | %.1f mV\n", sensors[i].label, raw, to_mv(raw));
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
// OLD VERSION (Ccontinuous mode not supported)
/*#include "adc_read.h"
#include "esp_adc/adc_continuous.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
 A B
 C D
IC3:
 14 19
 6 4
IC4:
 8 10
 16 15
static adc_continuous_handle_t adc_handle = NULL;
QueueHandle_t pressureQueue = NULL; 

static const adc_digi_pattern_config_t patterns[8] = {
    { .atten = ADC_ATTEN_DB_12, .unit = ADC_UNIT_1, .channel = ADC_CHANNEL_5, .bit_width = ADC_BITWIDTH_12 }, // GPIO6  IC3_C
    { .atten = ADC_ATTEN_DB_12, .unit = ADC_UNIT_1, .channel = ADC_CHANNEL_3, .bit_width = ADC_BITWIDTH_12 }, // GPIO4  IC3_D
    { .atten = ADC_ATTEN_DB_12, .unit = ADC_UNIT_1, .channel = ADC_CHANNEL_7, .bit_width = ADC_BITWIDTH_12 }, // GPIO8  IC4_A
    { .atten = ADC_ATTEN_DB_12, .unit = ADC_UNIT_1, .channel = ADC_CHANNEL_9, .bit_width = ADC_BITWIDTH_12 }, // GPIO10 IC4_B
    { .atten = ADC_ATTEN_DB_12, .unit = ADC_UNIT_2, .channel = ADC_CHANNEL_3, .bit_width = ADC_BITWIDTH_12 }, // GPIO14 IC3_A
    { .atten = ADC_ATTEN_DB_12, .unit = ADC_UNIT_2, .channel = ADC_CHANNEL_8, .bit_width = ADC_BITWIDTH_12 }, // GPIO19 IC3_B
    { .atten = ADC_ATTEN_DB_12, .unit = ADC_UNIT_2, .channel = ADC_CHANNEL_5, .bit_width = ADC_BITWIDTH_12 }, // GPIO16 IC4_C
    { .atten = ADC_ATTEN_DB_12, .unit = ADC_UNIT_2, .channel = ADC_CHANNEL_4, .bit_width = ADC_BITWIDTH_12 }, // GPIO15 IC4_D
};
static const char* labels[8] = {
    "IC3_C(GPIO6)",  "IC3_D(GPIO4)",  "IC4_A(GPIO8)",  "IC4_B(GPIO10)",
    "IC3_A(GPIO14)", "IC3_B(GPIO19)", "IC4_C(GPIO16)", "IC4_D(GPIO15)",
};
static float to_mv(uint16_t raw) {
    return (raw / 4095.0f) * (1100.0f * 2.818f);
}
void adc_init(void) {
    pressureQueue = xQueueCreate(8, sizeof(uint16_t));

    adc_continuous_handle_cfg_t handle_cfg = {
        .max_store_buf_size = 1024,
        .conv_frame_size = 256,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&handle_cfg, &adc_handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 2000,
        .conv_mode      = ADC_CONV_ALTER_UNIT,       // ADC1 and ADC2 alternating
        .format         = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
        .pattern_num    = 8,
        .adc_pattern    = (adc_digi_pattern_config_t*)patterns,
    };

    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
}

void adc_task(void *pvParameters) {
    uint8_t result[512] = {0};  
    uint32_t ret_num = 0;

    uint32_t sum[2][10] = {0};  // [unit][channel]
    int      count[2][10] = {0};

    while (1) {
        esp_err_t ret = adc_continuous_read(adc_handle, result, sizeof(result), &ret_num, 0);

        if (ret == ESP_OK && ret_num > 0) {
            memset(sum, 0, sizeof(sum));
            memset(count, 0, sizeof(count));

            for (int i = 0; i < ret_num; i += sizeof(adc_digi_output_data_t)) {
                adc_digi_output_data_t *out = (adc_digi_output_data_t*)&result[i];
                uint8_t unit = out->type2.unit;  // 0=ADC1, 1=ADC2
                uint8_t ch   = out->type2.channel;
                if (unit < 2 && ch < 10) {
                    sum[unit][ch]   += out->type2.data;
                    count[unit][ch]++;
                }
            }
            // Print the 8 channels
            for (int i = 0; i < 8; i++) {
                uint8_t unit = patterns[i].unit - 1;  // ADC_UNIT_1=1 is index 0
                uint8_t ch   = patterns[i].channel;
                if (count[unit][ch] > 0) {
                    uint16_t avg = sum[unit][ch] / count[unit][ch];
                    printf("%s | raw: %u | %.1f mV\n", labels[i], avg, to_mv(avg));
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
*/