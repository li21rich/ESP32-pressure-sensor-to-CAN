
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"

// ADC Drivers
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define NUM_SENSORS 8

// --- Configuration ---
typedef struct {
    adc_unit_t    unit;
    adc_channel_t channel;
    const char*   label;
} sensor_cfg_t;

static const sensor_cfg_t sensors[NUM_SENSORS] = {
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
    // --- IC 3 ---
    { ADC_UNIT_2, ADC_CHANNEL_3, "IC3_A (GPIO14)" },
    { ADC_UNIT_2, ADC_CHANNEL_8, "IC3_B (GPIO19)" },
    { ADC_UNIT_1, ADC_CHANNEL_5, "IC3_C (GPIO6)"  },
    { ADC_UNIT_1, ADC_CHANNEL_3, "IC3_D (GPIO4)"  },
    // --- IC 4 ---
    { ADC_UNIT_1, ADC_CHANNEL_7, "IC4_A (GPIO8)"  },
    { ADC_UNIT_1, ADC_CHANNEL_9, "IC4_B (GPIO10)" },
    { ADC_UNIT_2, ADC_CHANNEL_5, "IC4_C (GPIO16)" },
    { ADC_UNIT_2, ADC_CHANNEL_4, "IC4_D (GPIO15)" },
};

static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_oneshot_unit_handle_t adc2_handle = NULL;
static adc_cali_handle_t         adc1_cali   = NULL;
static adc_cali_handle_t         adc2_cali   = NULL;

QueueHandle_t pressureQueue = NULL;
static bool calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle) 
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cfg = {
        .unit_id  = unit,
        .atten    = atten,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cfg, &handle);
    if (ret == ESP_OK) calibrated = true;
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cfg = {
        .unit_id  = unit,
        .atten    = atten,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_line_fitting(&cfg, &handle);
    if (ret == ESP_OK) calibrated = true;
#endif

    *out_handle = handle;
    return calibrated;
}

void adc_init(void) {
    pressureQueue = xQueueCreate(8, sizeof(int));

    // Init ADC Units
    adc_oneshot_unit_init_cfg_t init_config1 = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_unit_init_cfg_t init_config2 = { .unit_id = ADC_UNIT_2 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config2, &adc2_handle));

    // Config Channels
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten    = ADC_ATTEN_DB_12,
    };

    for (int i = 0; i < NUM_SENSORS; i++) {
        adc_oneshot_unit_handle_t h = (sensors[i].unit == ADC_UNIT_1) ? adc1_handle : adc2_handle;
        ESP_ERROR_CHECK(adc_oneshot_config_channel(h, sensors[i].channel, &config));
    }

    // Init Calibration
    calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_12, &adc1_cali);
    calibration_init(ADC_UNIT_2, ADC_ATTEN_DB_12, &adc2_cali);
}

void adc_task(void *pvParameters) {

    while (1) {
        printf("\n--- Sensor Readings ---\n");
        
        for (int i = 0; i < NUM_SENSORS; i++) {
            int raw = 0;
            int voltage_mv = 0;
            
            // Select  handles
            adc_oneshot_unit_handle_t unit_handle = (sensors[i].unit == ADC_UNIT_1) ? adc1_handle : adc2_handle;
            adc_cali_handle_t cali_handle = (sensors[i].unit == ADC_UNIT_1) ? adc1_cali : adc2_cali;

            // Read
            if (adc_oneshot_read(unit_handle, sensors[i].channel, &raw) == ESP_OK) {
                
                // Convert to Voltage
                if (cali_handle) {
                    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw, &voltage_mv));
                    
                    // Print Result
                    printf("%s | %4d mV\n", sensors[i].label, voltage_mv);

                    // Send to Queue (commented)
                    // xQueueSend(pressureQueue, &voltage_mv, 0);
                } else {
                    printf("%s | Raw: %d (No Calib)\n", sensors[i].label, raw);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
