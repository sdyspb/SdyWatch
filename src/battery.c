#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "battery.h"
#include "matrix_display.h"

#define BATTERY_ADC_CHANNEL ADC1_CHANNEL_7
#define ADC_WIDTH ADC_WIDTH_BIT_12
#define ADC_ATTEN ADC_ATTEN_DB_12

#define LOW_BAT_THRESHOLD_MV  1900
#define EXTERNAL_PWR_THRESHOLD_MV 2200

static uint32_t samples[3] = {0, 0, 0};
static int sample_index = 0;
static int samples_collected = 0;

static esp_adc_cal_characteristics_t adc_chars;
bool low_battery_mode = false;
bool external_power_mode = false;

static uint32_t read_battery_mv(void) {
    uint32_t adc_reading = adc1_get_raw(BATTERY_ADC_CHANNEL);
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars);
    return voltage_mv;
}

static uint32_t median_filter(uint32_t new_sample) {
    samples[sample_index] = new_sample;
    sample_index = (sample_index + 1) % 3;
    if (samples_collected < 3) samples_collected++;

    if (samples_collected < 3) return new_sample;

    uint32_t a = samples[0], b = samples[1], c = samples[2];
    if (a > b) { uint32_t t = a; a = b; b = t; }
    if (b > c) { uint32_t t = b; b = c; c = t; }
    if (a > b) { uint32_t t = a; a = b; b = t; }
    return b;
}

void battery_init(void) {
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(BATTERY_ADC_CHANNEL, ADC_ATTEN);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 0, &adc_chars);
    samples_collected = 0;
}

void battery_task(void *arg) {
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        uint32_t mv = read_battery_mv();
        uint32_t filtered = median_filter(mv);

        external_power_mode = (filtered >= EXTERNAL_PWR_THRESHOLD_MV);

        if (samples_collected >= 3) {
            if (filtered < LOW_BAT_THRESHOLD_MV) {
                if (!low_battery_mode) {
                    low_battery_mode = true;
                    if (matrix_display_is_test_active()) {
                        matrix_display_stop_test();
                    }
                    matrix_display_set_text("LOW");
                }
            } else {
                if (low_battery_mode) {
                    low_battery_mode = false;
                }
            }
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10000));
    }
}