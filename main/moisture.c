#include "driver/adc.h"
#include "esp_log.h"

#define TAG "MOISTURE"

void moisture_init(void) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
}

void moisture_read(double* moisture) {
    *moisture = (adc1_get_raw(ADC1_CHANNEL_0)) / 4095.0 * 100.0;
}