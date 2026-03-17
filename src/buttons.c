#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <buttons.h>
#include <buzzer.h>
#include <matrix_display.h>
#include <sleep.h>
#include <alarm.h>
#include <config.h>

extern void toggle_mode(void);
extern bool alarm_active;
extern void alarm_deactivate(void);

static const char *TAG = "BUTTONS";

uint32_t last_activity_time = 0;

static struct {
    uint8_t pin;
    bool last_raw_state;
    bool stable_state;
    TickType_t last_change_tick;
} buttons[2] = {
    { BUTTON1_GPIO, true, true, 0 },
    { BUTTON2_GPIO, true, true, 0 }
};

static TickType_t btn2_press_start = 0;
static bool btn2_long_press_handled = false;

void buttons_init(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON1_GPIO) | (1ULL << BUTTON2_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    for (int i = 0; i < 2; i++) {
        buttons[i].last_raw_state = gpio_get_level(buttons[i].pin);
        buttons[i].stable_state = buttons[i].last_raw_state;
        buttons[i].last_change_tick = xTaskGetTickCount();
    }
    ESP_LOGI(TAG, "Buttons initialized on GPIO%d and GPIO%d", BUTTON1_GPIO, BUTTON2_GPIO);
}

static void debounce_button(int idx) {
    bool raw = gpio_get_level(buttons[idx].pin);
    TickType_t now = xTaskGetTickCount();

    if (raw != buttons[idx].last_raw_state) {
        buttons[idx].last_raw_state = raw;
        buttons[idx].last_change_tick = now;
    } else {
        if ((now - buttons[idx].last_change_tick) >= pdMS_TO_TICKS(BUTTON_DEBOUNCE_TIME_MS)) {
            if (raw != buttons[idx].stable_state) {
                buttons[idx].stable_state = raw;
                bool pressed = (raw == 0);   // active low

                if (pressed) {
                    ESP_LOGI(TAG, "Button %d PRESSED", idx+1);
                    last_activity_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

                    if (idx == 1) { // PB2
                        btn2_press_start = xTaskGetTickCount();
                        btn2_long_press_handled = false;
                    }
                } else {
                    ESP_LOGI(TAG, "Button %d RELEASED", idx+1);
                    if (idx == 1 && !btn2_long_press_handled) {
                        TickType_t duration = xTaskGetTickCount() - btn2_press_start;
                        if (duration >= pdMS_TO_TICKS(500)) {
                            if (!button1_pressed()) {
                                toggle_mode();   // long press PB2 switches mode
                                btn2_long_press_handled = true;
                            }
                        }
                    }
                }
            }
        }
    }
}

bool button1_pressed(void) {
    return (buttons[0].stable_state == 0);
}

bool button2_pressed(void) {
    return (buttons[1].stable_state == 0);
}

void buttons_task(void *arg) {
    static bool last_both_pressed = false;
    static bool last_button1_pressed = false;

    while (1) {
        debounce_button(0);
        debounce_button(1);

        bool button1 = button1_pressed();
        bool button2 = button2_pressed();

        // Both buttons: vibration test + show "SDYWATCH V1.0"
        bool both_pressed = button1 && button2;
        if (both_pressed && !last_both_pressed) {
            buzzer_pulse(BUZZER_PULSE_MS);
            if (!matrix_display_is_test_active()) {
                matrix_display_start_test("SDYWATCH V1.0");
            }
        }
        last_both_pressed = both_pressed;

        // Single press of PB1 (rising edge): if alarm active -> cancel, else go to sleep
        if (button1 && !last_button1_pressed && !button2) {
            if (alarm_active) {
                alarm_deactivate();
                ESP_LOGI(TAG, "Alarm deactivated by button");
            } else {
                ESP_LOGI(TAG, "Button 1 pressed alone - entering deep sleep");
                while (button1_pressed()) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                vTaskDelay(pdMS_TO_TICKS(20));
                enter_sleep_by_button();
            }
        }
        last_button1_pressed = button1;

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}