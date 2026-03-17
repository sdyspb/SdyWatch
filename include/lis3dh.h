#ifndef LIS3DH_H
#define LIS3DH_H

#include <stdbool.h>

void lis3dh_init(void);
bool lis3dh_is_interrupted(void);
void lis3dh_print_debug(void);
void lis3dh_check_clicks(void);

#endif // LIS3DH_H