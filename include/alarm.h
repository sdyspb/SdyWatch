#ifndef ALARM_H
#define ALARM_H

#include <stdbool.h>

void alarm_init(void);
void alarm_task(void *arg);
void alarm_deactivate(void);   // cancel active alarm

extern bool alarm_active;
extern int active_alarm_index; // -1 if none

#endif // ALARM_H