#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <buzzer.h>
#include <config.h>

static bool is_busy = false;
static esp_timer_handle_t pulse_timer = NULL;

static void pulse_off_callback(void *arg) {
    gpio_set_level(BUZZER_GPIO, 0);
    if (pulse_timer) {
        esp_timer_delete(pulse_timer);
        pulse_timer = NULL;
    }
    is_busy = false;
}

void buzzer_init(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << BUZZER_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);
    gpio_set_level(BUZZER_GPIO, 0);   // off by default
}

void buzzer_pulse(uint32_t duration_ms) {
    if (is_busy) return;   // ignore if already vibrating

    esp_timer_create_args_t timer_args = {
        .callback = pulse_off_callback,
        .name = "buzzer_pulse_off"
    };
    esp_err_t ret = esp_timer_create(&timer_args, &pulse_timer);
    if (ret != ESP_OK) {
        printf("Failed to create buzzer timer\n");
        return;
    }

    gpio_set_level(BUZZER_GPIO, 1);
    is_busy = true;
    esp_timer_start_once(pulse_timer, duration_ms * 1000);
}