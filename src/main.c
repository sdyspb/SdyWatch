#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "linenoise/linenoise.h"
#include <led_matrix.h>
#include <matrix_display.h>
#include <buttons.h>
#include <lis3dh.h>
#include <buzzer.h>
#include <ntp_task.h>
#include <compass.h>
#include <battery.h>
#include <alarm.h>
#include <alarm_config.h>
#include <config.h>
#include <clock_rtc.h>
#include <i2c.h>
#include <sleep.h>

extern uint32_t last_activity_time;
extern volatile bool ntp_active;
extern SemaphoreHandle_t ntp_start_semaphore;

RTC_DATA_ATTR int current_mode = 0;
bool mode_changed = false;

void toggle_mode(void);
void inactivity_task(void *arg);
void accel_task(void *arg);

#ifndef INACTIVITY_TIMEOUT_MS
#define INACTIVITY_TIMEOUT_MS 20000
#endif

void inactivity_task(void *arg) {
    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (!external_power_mode && (now - last_activity_time > INACTIVITY_TIMEOUT_MS)) {
            printf("Inactivity timeout reached, entering deep sleep.\n");
            enter_sleep_by_button();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void accel_task(void *arg) {
    uint32_t last_debug = 0;
    uint32_t interrupt_count = 0;
    static int last_second = -1;
    static bool first_time_update = true;

    if (ntp_start_semaphore != NULL) {
        xSemaphoreTake(ntp_start_semaphore, portMAX_DELAY);
    }

    while (1) {
        if (lis3dh_is_interrupted()) {
            interrupt_count++;
        }
        lis3dh_check_clicks();

        struct timeval tv;
        gettimeofday(&tv, NULL);
        time_t now = tv.tv_sec;
        struct tm *t = localtime(&now);

        if (!ntp_active && current_mode == 0 && !low_battery_mode && !matrix_display_is_test_active() && !alarm_active && t->tm_sec != last_second) {
            last_second = t->tm_sec;
            char time_str[32];
            if (t->tm_hour < 10) {
                sprintf(time_str, "%d:%02d.%02d", t->tm_hour, t->tm_min, t->tm_sec);
            } else {
                sprintf(time_str, "%02d:%02d.%02d", t->tm_hour, t->tm_min, t->tm_sec);
            }
            if (first_time_update) {
                matrix_display_set_text(time_str);
                first_time_update = false;
            } else {
                matrix_display_set_text_no_reset(time_str);
            }
        }

        static int last_level = -1;
        int level = gpio_get_level(ACCEL_INT_GPIO);
        if (level != last_level) {
            last_level = level;
        }

        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now_ms - last_debug > 5000) {
            last_debug = now_ms;
            lis3dh_print_debug();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void toggle_mode(void) {
    current_mode = !current_mode;
    mode_changed = true;
    printf("Switched to %s mode\n", current_mode ? "COMPASS" : "CLOCK");
}

void app_main(void) {
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

    ntp_start_semaphore = xSemaphoreCreateBinary();

    if (wakeup_cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
        clock_rtc_init();
        xTaskCreatePinnedToCore(ntp_task, "ntp_task", 4096, NULL, 2, NULL, 0);
    } else {
        printf("Wakeup from deep sleep (cause %d), RTC time preserved. NTP skipped.\n", wakeup_cause);
        xSemaphoreGive(ntp_start_semaphore);
    }

    setenv("TZ", "MSK-3", 1);
    tzset();

    i2c_master_init();
    led_matrix_init();
    matrix_display_init();
    buttons_init();
    lis3dh_init();
    buzzer_init();
    compass_init();
    compass_set_calibration(-620, 117, -18, 0.000426f, 0.000441f, 0.000413f);
    battery_init();
    alarm_config_init();
    register_alarm_commands();
    alarm_init();

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "watch>";
    repl_config.max_cmdline_length = 256;
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    xTaskCreatePinnedToCore(led_matrix_task, "led_matrix", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(buttons_task, "buttons", 2048, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(accel_task, "accel", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(inactivity_task, "inactivity", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(battery_task, "battery", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(alarm_task, "alarm", 2048, NULL, 1, NULL, 0);

    printf("System fully started. Current mode: %s. Will sleep after %d seconds of inactivity.\n",
           current_mode ? "COMPASS" : "CLOCK", INACTIVITY_TIMEOUT_MS/1000);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}