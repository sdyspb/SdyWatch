#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "compass.h"
#include "config.h"

static const char *TAG = "COMPASS";

#define QMC5883L_REG_X_LSB      0x00
#define QMC5883L_REG_X_MSB      0x01
#define QMC5883L_REG_Y_LSB      0x02
#define QMC5883L_REG_Y_MSB      0x03
#define QMC5883L_REG_Z_LSB      0x04
#define QMC5883L_REG_Z_MSB      0x05
#define QMC5883L_REG_STATUS      0x06
#define QMC5883L_REG_TEMP_LSB    0x07
#define QMC5883L_REG_TEMP_MSB    0x08
#define QMC5883L_REG_CONTROL1    0x09
#define QMC5883L_REG_CONTROL2    0x0A
#define QMC5883L_REG_PERIOD      0x0B

#define QMC5883L_MODE_CONTINUOUS 0x01
#define QMC5883L_ODR_200HZ       0x0C
#define QMC5883L_RNG_8G          0x10
#define QMC5883L_OSR_512         0x00
#define QMC5883L_SOFT_RST        0x80
#define QMC5883L_STATUS_DRDY     0x01

typedef struct {
    int16_t offset_x;
    int16_t offset_y;
    int16_t offset_z;
    float scale_x;
    float scale_y;
    float scale_z;
} compass_calibration_t;

static compass_calibration_t cal = {
    .offset_x = 0,
    .offset_y = 0,
    .offset_z = 0,
    .scale_x = 1.0f,
    .scale_y = 1.0f,
    .scale_z = 1.0f
};

static esp_err_t qmc5883l_write_reg(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (QMC5883L_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t qmc5883l_read_regs(uint8_t reg, uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (QMC5883L_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (QMC5883L_I2C_ADDRESS << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

void compass_init(void) {
    qmc5883l_write_reg(QMC5883L_REG_CONTROL2, QMC5883L_SOFT_RST);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t ctrl1 = QMC5883L_MODE_CONTINUOUS | QMC5883L_ODR_200HZ | QMC5883L_RNG_8G | QMC5883L_OSR_512;
    esp_err_t err = qmc5883l_write_reg(QMC5883L_REG_CONTROL1, ctrl1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure QMC5883L: %s", esp_err_to_name(err));
        return;
    }

    qmc5883l_write_reg(QMC5883L_REG_CONTROL2, 0x00);
    qmc5883l_write_reg(QMC5883L_REG_PERIOD, 0x01);
}

void compass_set_calibration(int16_t ox, int16_t oy, int16_t oz,
                             float sx, float sy, float sz) {
    cal.offset_x = ox;
    cal.offset_y = oy;
    cal.offset_z = oz;
    cal.scale_x = sx;
    cal.scale_y = sy;
    cal.scale_z = sz;
}

bool compass_read_heading(float *heading_deg) {
    uint8_t status;
    if (qmc5883l_read_regs(QMC5883L_REG_STATUS, &status, 1) != ESP_OK) {
        return false;
    }
    if (!(status & QMC5883L_STATUS_DRDY)) {
        return false;
    }

    uint8_t data[6];
    if (qmc5883l_read_regs(QMC5883L_REG_X_LSB, data, 6) != ESP_OK) {
        return false;
    }

    int16_t x_raw = (int16_t)(data[1] << 8 | data[0]);
    int16_t y_raw = (int16_t)(data[3] << 8 | data[2]);
    // z not used for heading

    float x_cal = (x_raw - cal.offset_x) * cal.scale_x;
    float y_cal = (y_raw - cal.offset_y) * cal.scale_y;

    float angle = atan2f(y_cal, x_cal) * 180.0f / 3.14159f;
    if (angle < 0) angle += 360.0f;

    *heading_deg = angle;
    return true;
}

void compass_calibrate(void) {
    // stub for future use
}