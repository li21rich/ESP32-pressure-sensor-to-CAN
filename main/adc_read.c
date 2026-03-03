/**
* Author: Richard Li
* Editors: Richard Li
*/

#include "adc_read.h" 
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"


#define ADC_MAX_RAW       4095.0f
#define ADC_MAX_VOLTAGE_MV 3100.0f

// Reduced to the 5 working pins. Other 3 (ADC2 GPIOs 14, 16, 19) removed due to conflicts (JTAG/Strap/WiFi).
#define NUM_SENSORS 5

typedef struct {
    adc_unit_t    unit;
    adc_channel_t channel;
    const char*   label;
} sensor_cfg_t;

static const sensor_cfg_t sensors[NUM_SENSORS] = {
    /*
    A B
    C D
    IO pin layouts
    IC3:
     9 11
     6 4
    IC4:
     15 17
     8 3
    */
    // IC 3 
    { ADC_UNIT_1, ADC_CHANNEL_8, "IC3_A (GPIO9)"  }, // A
    { ADC_UNIT_2, ADC_CHANNEL_1, "IC3_B (GPIO11)"  }, // B
    { ADC_UNIT_1, ADC_CHANNEL_5, "IC3_C (GPIO6)"  }, // C
    { ADC_UNIT_1, ADC_CHANNEL_3, "IC3_D (GPIO4)"  }, // D
    // IC 4
    { ADC_UNIT_2, ADC_CHANNEL_4, "IC4_A (GPIO15)"  }, // A
    { ADC_UNIT_2, ADC_CHANNEL_6, "IC4_B (GPIO17)"  }, // B
    { ADC_UNIT_1, ADC_CHANNEL_7, "IC4_C (GPIO8)"  }, // C
    { ADC_UNIT_1, ADC_CHANNEL_2, "IC4_D (GPIO3)"  }, // D

    // prevoiusly was ADC 1: 4 6 8 10 safe,
    // ADC 2: 15 safe
    // ADC 2: 14 16 19 bad (19 worst) due to hardware conflicts
    // pins were translated wrong and chosen wrong.
};

static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_oneshot_unit_handle_t adc2_handle = NULL;
static adc_cali_handle_t         adc1_cali   = NULL;
static adc_cali_handle_t         adc2_cali   = NULL;

QueueHandle_t pressureQueue = NULL;

static bool Calibrate(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle) 
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    printf("Attempting Curve Fitting calibration for Unit %d...\n", unit + 1);
    
    adc_cali_curve_fitting_config_t cfg = {
        .unit_id  = unit,
        .atten    = atten,
        .bitwidth = ADC_BITWIDTH_12,
    };
    
    ret = adc_cali_create_scheme_curve_fitting(&cfg, &handle);

    if (ret == ESP_OK) {
        *out_handle = handle;
        printf("Calibration Success (Curve Fitting) for Unit %d\n", unit + 1);
        return true;
    } 
    
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        printf("eFuse not burnt on this S3 module. Hardware calibration unavailable.\n");
    } else {
        printf("Calibration failed (Error: %s)\n", esp_err_to_name(ret));
    }

    *out_handle = NULL;
    return false;
}

void AdcInit(void) 
{
    pressureQueue = xQueueCreate(8, sizeof(int));

    // Init ADC Units
    adc_oneshot_unit_init_cfg_t init_config1 = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_unit_init_cfg_t init_config2 = { .unit_id = ADC_UNIT_2, .ulp_mode = ADC_ULP_MODE_DISABLE };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config2, &adc2_handle));

    // Init Calibration
    Calibrate(ADC_UNIT_1, ADC_ATTEN_DB_12, &adc1_cali);
    Calibrate(ADC_UNIT_2, ADC_ATTEN_DB_12, &adc2_cali);

    // Configure the channels
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten    = ADC_ATTEN_DB_12,
    };
    
    adc_oneshot_unit_handle_t h = NULL;

    // 5 sensors
    for (int i = 0; i < NUM_SENSORS; i++) {
        h = (sensors[i].unit == ADC_UNIT_1) ? adc1_handle : adc2_handle;
        ESP_ERROR_CHECK(adc_oneshot_config_channel(h, sensors[i].channel, &config));
    }
    
    printf("ADC Init Complete. Sensors active: %d\n", NUM_SENSORS);
}

void AdcTask(void *pvParameters) 
{
    int raw = 0;
    int voltage_mv = 0;
    adc_oneshot_unit_handle_t unit_handle = NULL;
    adc_cali_handle_t cali_handle = NULL;
    esp_err_t ret = ESP_FAIL;

    while (1) {
        printf("\n");
        for (int i = 0; i < NUM_SENSORS; i++) {
            raw = 0;
            voltage_mv = 0;
            unit_handle = (sensors[i].unit == ADC_UNIT_1) ? adc1_handle : adc2_handle;
            cali_handle = (sensors[i].unit == ADC_UNIT_1) ? adc1_cali : adc2_cali;

            // raw read happens right here
            ret = adc_oneshot_read(unit_handle, sensors[i].channel, &raw);

            // converting raw ADC readings to mV
            if (ret == ESP_OK) {
                // hardware calibration calculation
                if (cali_handle) {
                    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw, &voltage_mv));
                } else { // backup plan raw math calculation
                    // Approx: (Raw / Max_ADC) * Max_Voltage_Atten
                    // 11db attenuation on S3 is approx 3100mV max
                    voltage_mv = (int)((raw / ADC_MAX_RAW) * ADC_MAX_VOLTAGE_MV);
                }

                printf("%d. %s | Raw: %4d | %4d mV\n", i+1, sensors[i].label, raw, voltage_mv);

                // Send to Queue
                // xQueueSend(pressureQueue, &voltage_mv, 0);
            } else {
                printf("Read Error on %s: %s\n", sensors[i].label, esp_err_to_name(ret));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}