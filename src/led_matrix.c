#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "sys/time.h"
#include <led_matrix.h>
#include <matrix_display.h>
#include <compass.h>
#include <config.h>
#include <battery.h>
#include <alarm.h>

// External variables from main.c
extern int current_mode;
extern bool mode_changed;
extern bool low_battery_mode;
extern bool alarm_active;

static const uint8_t charlie_pins[CHARLIE_PIN_COUNT] = CHARLIE_PINS;
static led_t leds[TOTAL_LEDS];

// Fast access pointers to LED groups
static led_t* minute_leds[MINUTE_LEDS];
static led_t* hour_day_leds[HOUR_DAY_LEDS];
static led_t* hour_night_leds[HOUR_NIGHT_LEDS];
static led_t* matrix_leds[MATRIX_LEDS];

// ------------------------------------------------------------
// Build LED map according to Charlieplexing scheme
// ------------------------------------------------------------
static void build_led_map(void) {
    int idx = 0;
    int minute_idx = 0, hour_day_idx = 0, hour_night_idx = 0, matrix_idx = 0;

    // ----- Minute LEDs (60 pcs) -----
    const uint8_t minute_cathodes[5] = {CS0_GPIO, CS1_GPIO, CS2_GPIO, CS3_GPIO, CS4_GPIO};
    const uint8_t minute_anodes[5][12] = {
        {CS1_GPIO, CS2_GPIO, CS3_GPIO, CS4_GPIO, CS5_GPIO, CS6_GPIO, CS7_GPIO, CS8_GPIO, CS9_GPIO, CS10_GPIO, CS11_GPIO, CS12_GPIO},
        {CS0_GPIO, CS2_GPIO, CS3_GPIO, CS4_GPIO, CS5_GPIO, CS6_GPIO, CS7_GPIO, CS8_GPIO, CS9_GPIO, CS10_GPIO, CS11_GPIO, CS12_GPIO},
        {CS0_GPIO, CS1_GPIO, CS3_GPIO, CS4_GPIO, CS5_GPIO, CS6_GPIO, CS7_GPIO, CS8_GPIO, CS9_GPIO, CS10_GPIO, CS11_GPIO, CS12_GPIO},
        {CS0_GPIO, CS1_GPIO, CS2_GPIO, CS4_GPIO, CS5_GPIO, CS6_GPIO, CS7_GPIO, CS8_GPIO, CS9_GPIO, CS10_GPIO, CS11_GPIO, CS12_GPIO},
        {CS0_GPIO, CS1_GPIO, CS2_GPIO, CS3_GPIO, CS5_GPIO, CS6_GPIO, CS7_GPIO, CS8_GPIO, CS9_GPIO, CS10_GPIO, CS11_GPIO, CS12_GPIO}
    };
    for (int g = 0; g < 5; g++) {
        for (int i = 0; i < 12; i++) {
            leds[idx].cathode = minute_cathodes[g];
            leds[idx].anode   = minute_anodes[g][i];
            leds[idx].type    = LED_TYPE_MINUTE;
            leds[idx].index   = g * 12 + i;
            minute_leds[minute_idx++] = &leds[idx];
            idx++;
        }
    }

    // ----- Day hour LEDs (12 pcs) -----
    const uint8_t hour_day_anodes[12] = {
        CS0_GPIO, CS1_GPIO, CS2_GPIO, CS3_GPIO, CS4_GPIO,
        CS13_GPIO, CS7_GPIO, CS8_GPIO, CS9_GPIO, CS10_GPIO, CS11_GPIO, CS12_GPIO
    };
    for (int i = 0; i < 12; i++) {
        leds[idx].cathode = CS5_GPIO;
        leds[idx].anode   = hour_day_anodes[i];
        leds[idx].type    = LED_TYPE_HOUR_DAY;
        leds[idx].index   = i;
        hour_day_leds[hour_day_idx++] = &leds[idx];
        idx++;
    }

    // ----- Night hour LEDs (12 pcs) -----
    const uint8_t hour_night_anodes[12] = {
        CS0_GPIO, CS1_GPIO, CS2_GPIO, CS3_GPIO, CS4_GPIO,
        CS13_GPIO, CS7_GPIO, CS8_GPIO, CS9_GPIO, CS10_GPIO, CS11_GPIO, CS12_GPIO
    };
    for (int i = 0; i < 12; i++) {
        leds[idx].cathode = CS6_GPIO;
        leds[idx].anode   = hour_night_anodes[i];
        leds[idx].type    = LED_TYPE_HOUR_NIGHT;
        leds[idx].index   = i;
        hour_night_leds[hour_night_idx++] = &leds[idx];
        idx++;
    }

    // ----- Matrix 7x13 (91 pcs) -----
    const uint8_t matrix_cathodes[7] = {CS7_GPIO, CS8_GPIO, CS9_GPIO, CS10_GPIO, CS11_GPIO, CS12_GPIO, CS13_GPIO};
    for (int r = 0; r < 7; r++) {
        uint8_t cath = matrix_cathodes[r];
        for (int p = 0; p < CHARLIE_PIN_COUNT; p++) {
            uint8_t an = charlie_pins[p];
            if (an == cath) continue;
            leds[idx].cathode = cath;
            leds[idx].anode   = an;
            leds[idx].type    = LED_TYPE_MATRIX;
            leds[idx].index   = r * 13 + (matrix_idx % 13);
            matrix_leds[matrix_idx++] = &leds[idx];
            idx++;
        }
    }

    if (idx != TOTAL_LEDS) {
        printf("Error: LED count mismatch! %d vs %d\n", idx, TOTAL_LEDS);
    }
}

// ------------------------------------------------------------
// LED control
// ------------------------------------------------------------
void all_hi_z(void) {
    for (int i = 0; i < CHARLIE_PIN_COUNT; i++) {
        gpio_set_direction(charlie_pins[i], GPIO_MODE_INPUT);
    }
}

static void set_led(uint8_t cath, uint8_t an) {
    for (int i = 0; i < CHARLIE_PIN_COUNT; i++) {
        uint8_t pin = charlie_pins[i];
        if (pin == cath) {
            gpio_set_direction(pin, GPIO_MODE_OUTPUT);
            gpio_set_level(pin, 0);
        } else if (pin == an) {
            gpio_set_direction(pin, GPIO_MODE_OUTPUT);
            gpio_set_level(pin, 1);
        } else {
            gpio_set_direction(pin, GPIO_MODE_INPUT);
        }
    }
}

void led_matrix_init(void) {
    for (int i = 0; i < CHARLIE_PIN_COUNT; i++) {
        gpio_reset_pin(charlie_pins[i]);
        gpio_set_direction(charlie_pins[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(charlie_pins[i], GPIO_FLOATING);
    }
    build_led_map();
}

// ------------------------------------------------------------
// Main dynamic display task
// ------------------------------------------------------------
void led_matrix_task(void *arg) {
    uint32_t last_scroll_ms = 0;
    uint32_t last_compass_read = 0;
    float compass_angle = 0.0f;
    static float last_compass_angle = -1.0f;
    static char compass_text[16] = "";

    // At startup, if in compass mode and battery normal, not test, not alarm, show direction
    if (current_mode == 1 && !low_battery_mode && !matrix_display_is_test_active() && !alarm_active) {
        if (compass_read_heading(&compass_angle)) {
            last_compass_angle = compass_angle;
            int angle_int = (int)compass_angle;
            const char* dir;
            if (angle_int >= 338 || angle_int < 23) dir = "N";
            else if (angle_int >= 23 && angle_int < 68) dir = "NE";
            else if (angle_int >= 68 && angle_int < 113) dir = "E";
            else if (angle_int >= 113 && angle_int < 158) dir = "SE";
            else if (angle_int >= 158 && angle_int < 203) dir = "S";
            else if (angle_int >= 203 && angle_int < 248) dir = "SW";
            else if (angle_int >= 248 && angle_int < 293) dir = "W";
            else dir = "NW";
            sprintf(compass_text, "%d%c%s", angle_int, 0xB0, dir);
            matrix_display_set_text(compass_text);   // enable scrolling
        }
    }

    while (1) {
        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now_ms - last_scroll_ms >= SCROLL_INTERVAL_MS) {
            last_scroll_ms = now_ms;
            matrix_display_update();
        }

        // If mode changed by button
        if (mode_changed) {
            if (current_mode == 1 && !low_battery_mode && !matrix_display_is_test_active() && !alarm_active) {
                last_compass_angle = -1.0f;
                if (compass_read_heading(&compass_angle)) {
                    last_compass_angle = compass_angle;
                    int angle_int = (int)compass_angle;
                    const char* dir;
                    if (angle_int >= 338 || angle_int < 23) dir = "N";
                    else if (angle_int >= 23 && angle_int < 68) dir = "NE";
                    else if (angle_int >= 68 && angle_int < 113) dir = "E";
                    else if (angle_int >= 113 && angle_int < 158) dir = "SE";
                    else if (angle_int >= 158 && angle_int < 203) dir = "S";
                    else if (angle_int >= 203 && angle_int < 248) dir = "SW";
                    else if (angle_int >= 248 && angle_int < 293) dir = "W";
                    else dir = "NW";
                    sprintf(compass_text, "%d%c%s", angle_int, 0xB0, dir);
                    matrix_display_set_text(compass_text);
                }
            }
            mode_changed = false;
        }

        // Read compass in compass mode (every 100 ms) if not LOW, test or alarm
        if (current_mode == 1 && !low_battery_mode && !matrix_display_is_test_active() && !alarm_active && now_ms - last_compass_read > 100) {
            last_compass_read = now_ms;
            if (compass_read_heading(&compass_angle)) {
                if (fabsf(compass_angle - last_compass_angle) >= 1.0f) {
                    last_compass_angle = compass_angle;
                    int angle_int = (int)compass_angle;
                    const char* dir;
                    if (angle_int >= 338 || angle_int < 23) dir = "N";
                    else if (angle_int >= 23 && angle_int < 68) dir = "NE";
                    else if (angle_int >= 68 && angle_int < 113) dir = "E";
                    else if (angle_int >= 113 && angle_int < 158) dir = "SE";
                    else if (angle_int >= 158 && angle_int < 203) dir = "S";
                    else if (angle_int >= 203 && angle_int < 248) dir = "SW";
                    else if (angle_int >= 248 && angle_int < 293) dir = "W";
                    else dir = "NW";
                    sprintf(compass_text, "%d%c%s", angle_int, 0xB0, dir);
                    matrix_display_set_text_no_reset(compass_text);
                }
            }
        }

        // Get current time for clock mode
        struct timeval tv;
        gettimeofday(&tv, NULL);
        time_t now = tv.tv_sec;
        struct tm *t = localtime(&now);
        int hour24 = t->tm_hour;
        int minute = t->tm_min;
        int second = t->tm_sec;
        int hour12 = hour24 % 12;
        if (hour12 == 0) hour12 = 12;
        int is_day = (hour24 >= 6 && hour24 < 18) ? 1 : 0;

        // Determine active hour LED index for clock mode
        int hour_pos;
        if (hour12 == 12) {
            hour_pos = 11;
        } else {
            hour_pos = hour12 - 1;
        }
        led_t* hour_led = is_day ? hour_day_leds[hour_pos] : hour_night_leds[hour_pos];

        // --- Minute LEDs (60 pcs) ---
        for (int i = 0; i < MINUTE_LEDS; i++) {
            led_t* led = minute_leds[i];
            int state;
            if (current_mode == 0 && !low_battery_mode && !matrix_display_is_test_active() && !alarm_active) {
                int idx = led->index;
                int dial_pos = (idx + 1) % 60;   // 0 = 12 o'clock
                int minute_lit = (minute >= dial_pos);
                int second_flash = (second == dial_pos) ? 1 : 0;
                state = minute_lit ^ second_flash;
            } else {
                state = 0;
            }
            if (state) {
                set_led(led->cathode, led->anode);
            } else {
                all_hi_z();
            }
            esp_rom_delay_us(LED_ON_TIME_MINUTE);
            all_hi_z();
        }

        // --- Matrix LEDs (91 pcs) ---
        // In any mode, show the current buffer content (time, azimuth, LOW, test, alarm)
        for (int i = 0; i < MATRIX_LEDS; i++) {
            led_t* led = matrix_leds[i];
            int row = led->index / 13;
            int col = led->index % 13;
            int state = matrix_display_get_pixel(row, col) ? 1 : 0;
            if (state) {
                set_led(led->cathode, led->anode);
            } else {
                all_hi_z();
            }
            esp_rom_delay_us(LED_ON_TIME_MATRIX);
            all_hi_z();
        }

        // --- Hour LEDs (single active) ---
        if (current_mode == 0 && !low_battery_mode && !matrix_display_is_test_active() && !alarm_active) {
            if (hour_led) {
                set_led(hour_led->cathode, hour_led->anode);
                esp_rom_delay_us(LED_ON_TIME_HOUR);
                all_hi_z();
            }
        } else if (current_mode == 1 && !low_battery_mode && !matrix_display_is_test_active() && !alarm_active) {
            // Compass mode: show direction with red (night) LEDs
            int idx = ( ((int)(compass_angle + 15) / 30) + 11 ) % 12;
            led_t* compass_led = hour_night_leds[idx];
            set_led(compass_led->cathode, compass_led->anode);
            esp_rom_delay_us(LED_ON_TIME_HOUR);
            all_hi_z();
        }
        // If alarm active, hour LEDs are off

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}