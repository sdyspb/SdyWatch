#include <stdio.h>
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sleep.h"
#include "led_matrix.h"
#include "config.h"

void enter_sleep_by_button(void) {
    printf("Entering deep sleep...\n");
    fflush(stdout);

    all_hi_z();

#if WAKEUP_SOURCE == WAKEUP_SOURCE_BUTTON
    #define WAKEUP_PIN BUTTON1_GPIO
    const int active_level = 0;   // button pulls low
#elif WAKEUP_SOURCE == WAKEUP_SOURCE_ACCEL
    #define WAKEUP_PIN ACCEL_INT_GPIO
    const int active_level = 1;   // accelerometer gives high pulse
#else
    #error "Invalid WAKEUP_SOURCE defined in config.h"
#endif

    // Wait for the wake‑up pin to become inactive
    int level = gpio_get_level(WAKEUP_PIN);
    if (level == active_level) {
        while (gpio_get_level(WAKEUP_PIN) == active_level) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_err_t err = esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, active_level);
    if (err != ESP_OK) {
        printf("Failed to enable ext0 wakeup: %s\n", esp_err_to_name(err));
    }

    esp_deep_sleep_start();
    // never returns
}