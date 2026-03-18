#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "matrix_display.h"
#include "buzzer.h"
#include "config.h"
#include "ntp_task.h"

static const char *TAG = "NTP_TASK";

volatile bool ntp_active = false;
SemaphoreHandle_t ntp_start_semaphore = NULL;

static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
static bool wifi_should_retry = true;
static bool ntp_synced = false;

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data);
static void time_sync_cb(struct timeval *tv);
static bool wifi_connect(void);
static bool ntp_sync_with_countdown(void);

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_should_retry) {
            // ESP_LOGI(TAG, "Wi-Fi disconnected, retrying..."); // отладка
            esp_wifi_connect();
        } else {
            // ESP_LOGI(TAG, "Wi-Fi disconnected (stopped)"); // отладка
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        // ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip)); // отладка
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void time_sync_cb(struct timeval *tv) {
    ntp_synced = true;
    // ESP_LOGI(TAG, "NTP time sync callback received"); // отладка
}

static bool wifi_connect(void) {
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id, instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // ESP_LOGI(TAG, "Connecting to %s", WIFI_SSID); // отладка

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(20000));
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

static bool ntp_sync_with_countdown(void) {
    ntp_synced = false;
    int retry = 0;
    const int max_retry = NTP_MAX_RETRY;
    char num_str[8];

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, NTP_SERVER1);
    esp_sntp_setservername(1, NTP_SERVER2);
    sntp_set_time_sync_notification_cb(time_sync_cb);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    esp_sntp_init();

    int remaining = max_retry - retry;
    sprintf(num_str, "%d", remaining);
    matrix_display_set_text(num_str);
    retry++;

    while (!ntp_synced && retry < max_retry) {
        vTaskDelay(pdMS_TO_TICKS(500));
        remaining = max_retry - retry;
        sprintf(num_str, "%d", remaining);
        matrix_display_set_text_no_reset(num_str);
        retry++;
    }

    if (ntp_synced) {
        vTaskDelay(pdMS_TO_TICKS(200));
        setenv("TZ", "MSK-3", 1);
        tzset();

        // time_t now;
        // struct tm timeinfo;
        // time(&now);
        // localtime_r(&now, &timeinfo);
        // char time_buf[64];
        // strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        // ESP_LOGI(TAG, "Current local time: %s", time_buf); // отладка

        buzzer_pulse(BUZZER_PULSE_MS);
        return true;
    } else {
        ESP_LOGE(TAG, "NTP sync timeout");
        return false;
    }
}

void ntp_task(void *pvParameters) {
    // ESP_LOGI(TAG, "Starting NTP task"); // отладка

    ntp_active = true;
    if (ntp_start_semaphore != NULL) {
        xSemaphoreGive(ntp_start_semaphore);
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_should_retry = true;
    bool wifi_ok = wifi_connect();
    if (wifi_ok) {
        ntp_sync_with_countdown();
        wifi_should_retry = false;
        esp_wifi_disconnect();
        esp_wifi_stop();
    } else {
        ESP_LOGE(TAG, "Wi-Fi connection failed");
    }

    ntp_active = false;
    vTaskDelete(NULL);
}

void start_ntp_task(void) {
    if (!ntp_active) {
        xTaskCreate(ntp_task, "ntp_task", 4096, NULL, 1, NULL);
    } else {
        printf("NTP sync already in progress\n");
    }
}