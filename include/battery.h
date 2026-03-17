#ifndef BATTERY_H
#define BATTERY_H

#include <stdbool.h>

void battery_init(void);
void battery_task(void *arg);

extern bool low_battery_mode;
extern bool external_power_mode;

#endif // BATTERY_H