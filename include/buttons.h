#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>

extern uint32_t last_activity_time;

void buttons_init(void);
void buttons_task(void *arg);
bool button1_pressed(void);
bool button2_pressed(void);

#endif // BUTTONS_H