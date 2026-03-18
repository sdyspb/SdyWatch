#ifndef CONFIG_H
#define CONFIG_H

// Charlieplexing pins (5 cathodes, 14 anodes)
#define CS0_GPIO     13
#define CS1_GPIO     14
#define CS2_GPIO     4
#define CS3_GPIO     5
#define CS4_GPIO     9
#define CS5_GPIO     10
#define CS6_GPIO     18
#define CS7_GPIO     19
#define CS8_GPIO     21
#define CS9_GPIO     22
#define CS10_GPIO    23
#define CS11_GPIO    25
#define CS12_GPIO    26
#define CS13_GPIO    27

#define CHARLIE_PINS  {13,14,4,5,9,10,18,19,21,22,23,25,26,27}
#define CHARLIE_PIN_COUNT 14

// ========== LED brightness (on-time in microseconds) ==========
#define LED_ON_TIME_MINUTE   70   // for minute LEDs (60 pcs)
#define LED_ON_TIME_HOUR     200  // for hour LEDs (one active)
#define LED_ON_TIME_MATRIX   30   // for matrix LEDs (91 pcs)

#define MINUTE_LEDS    60
#define HOUR_DAY_LEDS  12
#define HOUR_NIGHT_LEDS 12
#define MATRIX_LEDS    91
#define TOTAL_LEDS     (MINUTE_LEDS + HOUR_DAY_LEDS + HOUR_NIGHT_LEDS + MATRIX_LEDS)

// ========== I²C ==========
#define I2C_MASTER_SCL_GPIO    2
#define I2C_MASTER_SDA_GPIO    15
#define I2C_MASTER_FREQ_HZ     100000

// ========== Accelerometer ==========
#define ACCEL_INT_GPIO          37
#define LIS3DH_I2C_ADDRESS      0x19

// ========== Vibration motor ==========
#define BUZZER_GPIO             12
#define BUZZER_PULSE_MS         200

// ========== Buttons ==========
#define BUTTON1_GPIO            38   // RTC-capable, wakes from deep sleep
#define BUTTON2_GPIO            39
#define BUTTON_DEBOUNCE_TIME_MS 50

// ========== Compass QMC5883L ==========
#define QMC5883L_I2C_ADDRESS    0x0D   // 7‑bit address

// ========== Scrolling matrix ==========
#define SCROLL_SPEED_PPS        12      // pixels per second
#define SCROLL_INTERVAL_MS      (1000 / SCROLL_SPEED_PPS)

// ========== Sleep settings ==========
#define INACTIVITY_TIMEOUT_MS   30000   // idle time before sleep (ms)

// ========== Battery monitoring ==========
#define LOW_BAT_THRESHOLD_MV    1900    // ADC voltage corresponding to ~3.2V battery
#define EXTERNAL_PWR_THRESHOLD_MV 2200  // ADC voltage for external power (~3.7V battery)

// ========== Wi‑Fi and NTP ==========
#define WIFI_SSID       "Aliens"
#define WIFI_PASS       "Bishop 341-B"
#define WIFI_MAX_RETRY  5
#define NTP_SERVER1     "pool.ntp.org"
#define NTP_SERVER2     "time.nist.gov"
#define NTP_MAX_RETRY   20
#define NTP_TIMEOUT_MS  10000

// ========== Wake‑up source selection ==========
#define WAKEUP_SOURCE_BUTTON 1   // wake by PB1 (active low)
#define WAKEUP_SOURCE_ACCEL  2   // wake by accelerometer (active high)
#define WAKEUP_SOURCE WAKEUP_SOURCE_BUTTON  // choose before building

// ========== Initial RTC time (overwritten by NTP) ==========
#define INITIAL_HOUR    0
#define INITIAL_MINUTE  0
#define INITIAL_SECOND  0

#endif // CONFIG_H