#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "alarm.h"
#include "alarm_config.h"
#include "buzzer.h"
#include "matrix_display.h"
#include "config.h"
#include "esp_timer.h"
#include "sys/time.h"

bool alarm_active = false;
int active_alarm_index = -1;

static bool alarm_triggered_today[3] = {false, false, false};
static time_t alarm_start_time = 0;
static uint32_t last_vibro_time = 0;

static bool time_matches(const alarm_t *a, int hour, int minute) {
    return (a->hour == hour && a->minute == minute);
}

static void deactivate_alarm(void) {
    if (!alarm_active) return;
    alarm_active = false;
    if (active_alarm_index >= 0) {
        alarm_triggered_today[active_alarm_index] = true;
    }
    extern bool mode_changed;   // to restore normal display
    mode_changed = true;
}

void alarm_deactivate(void) {
    deactivate_alarm();
}

void alarm_init(void) {
    memset(alarm_triggered_today, 0, sizeof(alarm_triggered_today));
}

void alarm_task(void *arg) {
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        time_t now = tv.tv_sec;
        struct tm *tm = localtime(&now);

        int current_hour = tm->tm_hour;
        int current_min = tm->tm_min;
        int current_day = tm->tm_yday;

        // Reset daily flags at midnight
        static int last_day = -1;
        if (last_day != current_day) {
            last_day = current_day;
            memset(alarm_triggered_today, 0, sizeof(alarm_triggered_today));
        }

        if (alarm_active) {
            // Check if 1 minute elapsed
            if (now - alarm_start_time >= 60) {
                deactivate_alarm();
            } else {
                // If alarm was disabled via console
                if (active_alarm_index >= 0 && !alarm_config.alarms[active_alarm_index].enabled) {
                    deactivate_alarm();
                }
                // Vibration every 10 seconds
                uint32_t elapsed = now - alarm_start_time;
                if (elapsed / 10 > last_vibro_time / 10) {
                    last_vibro_time = elapsed;
                    buzzer_pulse(150);   // 150 ms pulse
                }
            }
        } else {
            // Check all alarms
            for (int i = 0; i < MAX_ALARMS; i++) {
                if (alarm_config.alarms[i].enabled && !alarm_triggered_today[i]) {
                    if (time_matches(&alarm_config.alarms[i], current_hour, current_min)) {
                        alarm_active = true;
                        active_alarm_index = i;
                        alarm_start_time = now;
                        last_vibro_time = 0;
                        matrix_display_set_text(alarm_config.alarms[i].message);
                        buzzer_pulse(150);
                        break;
                    }
                }
            }
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000));   // check every second
    }
}