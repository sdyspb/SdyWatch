#ifndef LED_MATRIX_H
#define LED_MATRIX_H

#include <stdint.h>

typedef enum {
    LED_TYPE_MINUTE,
    LED_TYPE_HOUR_DAY,
    LED_TYPE_HOUR_NIGHT,
    LED_TYPE_MATRIX
} led_type_t;

typedef struct {
    uint8_t cathode;
    uint8_t anode;
    led_type_t type;
    uint16_t index;
} led_t;

void led_matrix_init(void);
void led_matrix_task(void *arg);
void all_hi_z(void);   // set all Charlieplexing pins to high‑Z

#endif // LED_MATRIX_H