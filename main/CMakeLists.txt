idf_component_register(
    SRCS "main.c" "adc_read.c" "can_send.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos esp_adc 
)
