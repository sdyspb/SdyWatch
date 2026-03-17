#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <lis3dh.h>
#include <config.h>

static const char *TAG = "LIS3DH";

#define LIS3DH_REG_WHO_AM_I     0x0F
#define LIS3DH_REG_CTRL_REG1    0x20
#define LIS3DH_REG_CTRL_REG2    0x21
#define LIS3DH_REG_CTRL_REG3    0x22
#define LIS3DH_REG_CTRL_REG4    0x23
#define LIS3DH_REG_CTRL_REG5    0x24
#define LIS3DH_REG_CTRL_REG6    0x25
#define LIS3DH_REG_STATUS       0x27
#define LIS3DH_REG_OUT_X_L      0x28
#define LIS3DH_REG_OUT_X_H      0x29
#define LIS3DH_REG_OUT_Y_L      0x2A
#define LIS3DH_REG_OUT_Y_H      0x2B
#define LIS3DH_REG_OUT_Z_L      0x2C
#define LIS3DH_REG_OUT_Z_H      0x2D
#define LIS3DH_REG_CLICK_CFG    0x38
#define LIS3DH_REG_CLICK_SRC    0x39
#define LIS3DH_REG_CLICK_THS    0x3A
#define LIS3DH_REG_TIME_LIMIT   0x3B
#define LIS3DH_REG_TIME_LATENCY 0x3C
#define LIS3DH_REG_TIME_WINDOW  0x3D

#define LIS3DH_WHO_AM_I_VAL     0x33

static volatile bool s_interrupt_triggered = false;
static uint32_t interrupt_counter = 0;

static void IRAM_ATTR lis3dh_isr_handler(void* arg) {
    s_interrupt_triggered = true;
    interrupt_counter++;
}

static esp_err_t lis3dh_register_write(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LIS3DH_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t lis3dh_register_read(uint8_t reg, uint8_t *value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LIS3DH_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LIS3DH_I2C_ADDRESS << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t lis3dh_read_accel(int16_t *x, int16_t *y, int16_t *z) {
    uint8_t data[6];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LIS3DH_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, LIS3DH_REG_OUT_X_L | 0x80, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LIS3DH_I2C_ADDRESS << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_OK) {
        *x = (int16_t)(data[1] << 8 | data[0]);
        *y = (int16_t)(data[3] << 8 | data[2]);
        *z = (int16_t)(data[5] << 8 | data[4]);
    }
    return ret;
}

void lis3dh_init(void) {
    uint8_t who_am_i = 0;
    esp_err_t ret = lis3dh_register_read(LIS3DH_REG_WHO_AM_I, &who_am_i);
    if (ret != ESP_OK || who_am_i != LIS3DH_WHO_AM_I_VAL) {
        ESP_LOGE(TAG, "Failed to read WHO_AM_I (0x%02X). Error: %s", who_am_i, esp_err_to_name(ret));
        return;
    }

    ESP_ERROR_CHECK(lis3dh_register_write(LIS3DH_REG_CTRL_REG1, 0x4F)); // 400Hz, XYZ enabled
    ESP_ERROR_CHECK(lis3dh_register_write(LIS3DH_REG_CTRL_REG2, 0x00));
    ESP_ERROR_CHECK(lis3dh_register_write(LIS3DH_REG_CTRL_REG3, (1 << 7))); // I1_CLICK
    uint8_t ctrl3_check;
    lis3dh_register_read(LIS3DH_REG_CTRL_REG3, &ctrl3_check);

    ESP_ERROR_CHECK(lis3dh_register_write(LIS3DH_REG_CTRL_REG4, 0x10)); // ±8g
    ESP_ERROR_CHECK(lis3dh_register_write(LIS3DH_REG_CTRL_REG5, 0x00));
    ESP_ERROR_CHECK(lis3dh_register_write(LIS3DH_REG_CTRL_REG6, 0x00));

    ESP_ERROR_CHECK(lis3dh_register_write(LIS3DH_REG_CLICK_CFG, 0x15)); // all axes single/double click
    ESP_ERROR_CHECK(lis3dh_register_write(LIS3DH_REG_CLICK_THS, 0x1E)); // threshold 30
    ESP_ERROR_CHECK(lis3dh_register_write(LIS3DH_REG_TIME_LIMIT, 0x0A));
    ESP_ERROR_CHECK(lis3dh_register_write(LIS3DH_REG_TIME_LATENCY, 0x28));
    ESP_ERROR_CHECK(lis3dh_register_write(LIS3DH_REG_TIME_WINDOW, 0xFF));

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,   // active low (as per current hardware)
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << ACCEL_INT_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(ACCEL_INT_GPIO, lis3dh_isr_handler, NULL);
}

bool lis3dh_is_interrupted(void) {
    if (s_interrupt_triggered) {
        s_interrupt_triggered = false;
        uint8_t click_src;
        if (lis3dh_register_read(LIS3DH_REG_CLICK_SRC, &click_src) == ESP_OK) {
            // optional debug
            return true;
        }
    }
    return false;
}

void lis3dh_print_debug(void) {
    // optional debug function (can be left empty)
}

void lis3dh_check_clicks(void) {
    uint8_t click_src;
    if (lis3dh_register_read(LIS3DH_REG_CLICK_SRC, &click_src) == ESP_OK) {
        if (click_src & 0x10) {
            // single tap detected – can be used for activity reset
        }
        if (click_src & 0x20) {
            // double tap detected – future use
        }
    }
}