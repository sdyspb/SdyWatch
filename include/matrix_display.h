#ifndef MATRIX_DISPLAY_H
#define MATRIX_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

#define MATRIX_WIDTH  13
#define MATRIX_HEIGHT 7
#define CHAR_WIDTH  5
#define CHAR_SPACING 1
#define CHAR_TOTAL_WIDTH (CHAR_WIDTH + CHAR_SPACING)   // 6

void matrix_display_init(void);
bool matrix_display_get_pixel(uint8_t row, uint8_t col);
void matrix_display_update(void);
void matrix_display_set_text(const char *str);
void matrix_display_set_text_no_reset(const char *str);
void matrix_display_enable_scroll(bool enable);
bool matrix_display_is_scroll_enabled(void);
void matrix_display_save_state(void);
void matrix_display_restore_state(void);
void matrix_display_set_offset(int offset);

// For test mode
void matrix_display_start_test(const char *test_text);
bool matrix_display_is_test_active(void);
void matrix_display_stop_test(void);

#endif // MATRIX_DISPLAY_H