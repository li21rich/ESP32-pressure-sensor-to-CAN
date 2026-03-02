/**
* Author: Richard Li
* Editors: Richard Li
*/

#include <stdio.h>
#include "adc_read.h"
#include "can_send.h"
#include "freertos/FreeRTOS.h"

void app_main(void) {
    AdcInit();
    //can_init();

    xTaskCreate(AdcTask, "adc_task", 4096, NULL, 5, NULL);
    //xTaskCreate(can_task, "can_task", 4096, NULL, 6, NULL);

} 