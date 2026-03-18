# ESP32-PICO-D4 SdyWatch

Firmware for a custom smartwatch with analog LED display, scrolling matrix, compass, NTP time sync, alarms, and deep‑sleep power management.

<img width="1251" height="844" alt="image" src="https://github.com/user-attachments/assets/67f83ad0-4cf7-4457-8042-333f3df41961" />

## Features

- **Analog LED Display**  
  - 60 green LEDs for minutes/seconds (minute accumulation, second flash).  
  - 12+12 bicolor LEDs for hours (day/night colors, only one lit at a time).  
  - Central 7×13 LED matrix for scrolling text.

- **Scrolling Matrix**  
  - Shows current time `H:MM.SS` without leading zero (e.g., `8:05.30`).  
  - In compass mode: shows azimuth and direction (e.g., `123° N`).  
  - Low battery warning: `LOW`.  
  - Test mode: `SDYWATCH V1.0` (triggered by both buttons).

- **Buttons (PB1, PB2)**  
  - PB1: force deep sleep (single press) or cancel alarm.  
  - PB2 long press (500ms): switch between clock / compass mode.  
  - Both buttons simultaneously: test vibration + show test message.

- **Vibration Motor**  
  - Pulse on alarm, NTP success, button test. Configurable duration.

- **Accelerometer (LIS3DH)**  
  - Detects single taps (threshold adjustable).  
  - Tap resets inactivity timer (prevents sleep while moving).

- **Compass (QMC5883L)**  
  - Calibrated azimuth reading.  
  - Displays angle and 8‑point direction on matrix; red hour LED points north.

- **Battery Monitoring** (ADC1_CH7, GPIO34)  
  - Measures voltage every 10s with median filter (3 samples).  
  - Low battery threshold (ADC < 1900 mV ≈ 3.2 V): shows `LOW` and disables other LEDs.  
  - External power threshold (ADC ≥ 2200 mV ≈ 3.7 V): disables auto sleep.

- **NTP Time Synchronisation**  
  - On cold boot, connects to Wi‑Fi, obtains UTC, sets RTC.  
  - Countdown of remaining attempts (e.g., 40) shown on matrix.  
  - Vibrates on success, Wi‑Fi turned off afterwards.  
  - On wake‑up from sleep, NTP is skipped (RTC time used).

- **Deep Sleep**  
  - Automatic sleep after 20 seconds of inactivity (except when on external power).  
  - Wake‑up by PB1 (or accelerometer, configurable).  
  - Saves current mode (clock/compass) in RTC memory.

- **Three Alarms**  
  - Configurable via UART console.  
  - Each alarm has hour, minute, message, enable/disable flag.  
  - When triggered: message scrolls on matrix, vibration pulses every 10s for 1 minute.  
  - Cancel by PB1 or after the minute ends.

- **UART Console** (115200 baud)  
  - Built‑in REPL with commands:  
    - `alarm_show` – show all alarms  
    - `alarm_set <1..3> <hour> <min> [message]` – set alarm time and optional message  
    - `alarm_enable <1..3> <0/1>` – enable/disable alarm  
    - `help` – list all commands

- **Configurable Brightness**  
  - Separate `LED_ON_TIME_MINUTE`, `LED_ON_TIME_HOUR`, `LED_ON_TIME_MATRIX` in `config.h`.  
  - Refresh rate ≈ 60 Hz.

## Hardware Requirements

- ESP32‑PICO‑D4 module (or any ESP32 with enough GPIOs)
- 175 LEDs arranged as described (60 green, 12+12 bicolor, 91 for matrix)
- Charlieplexing matrix wiring (5 cathodes, 14 anodes – pins defined in `config.h`)
- External 32.768 kHz crystal for RTC (pins 32,33)
- Buttons: PB1 on GPIO38 (RTC-capable), PB2 on GPIO39
- LIS3DH accelerometer (I²C address 0x19, interrupt on GPIO37)
- QMC5883L compass (I²C address 0x0D)
- Vibration motor on GPIO12 (active high)
- Voltage divider (147k / 100k+147k) on GPIO34 for battery measurement

## Configuration

All user‑adjustable settings are in `include/config.h`. Key parameters:

- `LED_ON_TIME_MINUTE`, `LED_ON_TIME_HOUR`, `LED_ON_TIME_MATRIX` – brightness (µs)
- `SCROLL_SPEED_PPS` – scrolling speed (pixels per second)
- `INACTIVITY_TIMEOUT_MS` – idle time before sleep (ms)
- `LOW_BAT_THRESHOLD_MV` – ADC voltage for low battery (mV)
- `EXTERNAL_PWR_THRESHOLD_MV` – ADC voltage for external power detection
- `WIFI_SSID`, `WIFI_PASS` – your Wi‑Fi credentials
- `NTP_SERVER1`, `NTP_SERVER2` – NTP servers
- `WAKEUP_SOURCE` – choose `WAKEUP_SOURCE_BUTTON` or `WAKEUP_SOURCE_ACCEL`

## Console Usage Example

Connect to the UART (115200) and type:

```console
watch> alarm_show
=== Alarm Configuration ===
Alarm 1: DISABLED 07:00 "Alarm 1"
Alarm 2: DISABLED 07:00 "Alarm 2"
Alarm 3: DISABLED 07:00 "Alarm 3"

watch> alarm_set 1 8 30 "Good morning"
Alarm 1 set to 08:30 message: Good morning

watch> alarm_enable 1 1
Alarm 1 enabled

watch> alarm_show
Alarm 1: ENABLED 08:30 "Good morning"
Alarm 2: DISABLED 07:00 "Alarm 2"
Alarm 3: DISABLED 07:00 "Alarm 3"
```

## Building and Flashing

This project uses **PlatformIO** with the **ESP-IDF** framework.

1. Install [PlatformIO](https://platformio.org/) (VS Code extension or command‑line).
2. Open the project folder.
3. Build: `pio run`
4. Upload: `pio run -t upload` (connect via USB‑UART, press BOOT+EN to enter download mode).
5. Monitor: `pio device monitor -b 115200`

