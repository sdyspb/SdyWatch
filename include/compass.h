#ifndef COMPASS_H
#define COMPASS_H

#include <stdint.h>
#include <stdbool.h>

void compass_init(void);
bool compass_read_heading(float *heading_deg);
void compass_set_calibration(int16_t ox, int16_t oy, int16_t oz,
                             float sx, float sy, float sz);
void compass_calibrate(void);   // stub

#endif // COMPASS_H