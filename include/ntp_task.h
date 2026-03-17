#ifndef NTP_TASK_H
#define NTP_TASK_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern volatile bool ntp_active;
extern SemaphoreHandle_t ntp_start_semaphore;

void start_ntp_task(void);
void ntp_task(void *pvParameters);

#endif // NTP_TASK_H