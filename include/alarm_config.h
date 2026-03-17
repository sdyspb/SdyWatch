#ifndef ALARM_CONFIG_H
#define ALARM_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_ALARMS 3
#define ALARM_MSG_LEN 32

typedef struct {
    uint8_t hour;
    uint8_t minute;
    bool enabled;
    char message[ALARM_MSG_LEN];
} alarm_t;

typedef struct {
    alarm_t alarms[MAX_ALARMS];
} alarm_config_t;

extern alarm_config_t alarm_config;

void alarm_config_init(void);
void register_alarm_commands(void);

#endif // ALARM_CONFIG_H