#include "matrix_display.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define INITIAL_OFFSET (-13)

// 5x7 font for characters '0'-'9', ':', '.', '°', 'N','S','E','W', 'D','Y','A','T','C','H','V','L', space
static const uint8_t font[26][5] = {
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 0
    {0x00, 0x21, 0x7F, 0x01, 0x00}, // 1
    {0x23, 0x45, 0x49, 0x51, 0x22}, // 2
    {0x22, 0x41, 0x49, 0x49, 0x36}, // 3
    {0x0C, 0x14, 0x24, 0x7F, 0x04}, // 4
    {0x72, 0x51, 0x51, 0x51, 0x4E}, // 5
    {0x3E, 0x49, 0x49, 0x49, 0x06}, // 6
    {0x40, 0x40, 0x47, 0x48, 0x70}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x30, 0x49, 0x49, 0x4A, 0x3C}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // 10 ':'
    {0x00, 0x00, 0x08, 0x00, 0x00}, // 11 '.'
    {0x30, 0x48, 0x48, 0x30, 0x00}, // 12 '°'
    // Compass letters (indices 13-16)
    {0x7F, 0x10, 0x08, 0x04, 0x7F}, // 13 'N'
    {0x32, 0x49, 0x49, 0x49, 0x26}, // 14 'S'
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 15 'E'
    {0x7E, 0x01, 0x1E, 0x01, 0x7E}, // 16 'W'
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 17 space
    // Letters for "SDYWATCH V1.0" and "LOW"
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 18 'D'
    {0x40, 0x20, 0x1F, 0x20, 0x40}, // 19 'Y'
    {0x1F, 0x24, 0x44, 0x24, 0x1F}, // 20 'A'
    {0x40, 0x40, 0x7F, 0x40, 0x40}, // 21 'T'
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 22 'C'
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 23 'H'
    {0x7C, 0x02, 0x01, 0x02, 0x7C}, // 24 'V'
    {0x7F, 0x01, 0x01, 0x01, 0x02}  // 25 'L'
};

static char text[16] = "";
static int text_len = 0;
static int scroll_offset = INITIAL_OFFSET;
static bool frame_buffer[MATRIX_HEIGHT][MATRIX_WIDTH];
static SemaphoreHandle_t text_mutex = NULL;
static bool scroll_enabled = false;
static int saved_scroll_offset;
static bool saved_scroll_enabled;

// Test mode variables
static bool test_mode_active = false;
static char saved_normal_text[16] = "";
static int saved_normal_len = 0;

static int char_to_font_index(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c == ':') return 10;
    if (c == '.') return 11;
    if ((unsigned char)c == 0xB0) return 12;   // degree symbol
    if (c == 'N') return 13;
    if (c == 'S') return 14;
    if (c == 'E') return 15;
    if (c == 'W') return 16;
    if (c == ' ') return 17;
    if (c == 'D') return 18;
    if (c == 'Y') return 19;
    if (c == 'A') return 20;
    if (c == 'T') return 21;
    if (c == 'C') return 22;
    if (c == 'H') return 23;
    if (c == 'V') return 24;
    if (c == 'L') return 25;
    return 0;   // default '0'
}

static void render_frame(void) {
    memset(frame_buffer, 0, sizeof(frame_buffer));
    if (text_len == 0) return;

    char local_text[16];
    int len;
    if (text_mutex) {
        xSemaphoreTake(text_mutex, portMAX_DELAY);
        len = text_len;
        memcpy(local_text, text, len);
        xSemaphoreGive(text_mutex);
    } else {
        len = text_len;
        memcpy(local_text, text, len);
    }
    if (len == 0) return;

    int max_offset = len * CHAR_TOTAL_WIDTH;
    for (int col = 0; col < MATRIX_WIDTH; col++) {
        int pixel_index = scroll_offset + col;
        if (pixel_index < 0 || pixel_index >= max_offset) continue;
        int char_index = (pixel_index / CHAR_TOTAL_WIDTH) % len;
        int pixel_in_char = pixel_index % CHAR_TOTAL_WIDTH;
        if (pixel_in_char < CHAR_WIDTH) {
            char c = local_text[char_index];
            int font_idx = char_to_font_index(c);
            uint8_t char_col = font[font_idx][pixel_in_char];
            for (int row = 0; row < MATRIX_HEIGHT; row++) {
                if (char_col & (1 << (6 - row))) {
                    frame_buffer[row][col] = true;
                }
            }
        }
    }
}

void matrix_display_init(void) {
    text_mutex = xSemaphoreCreateMutex();
    scroll_offset = INITIAL_OFFSET;
    scroll_enabled = false;
    render_frame();
}

bool matrix_display_get_pixel(uint8_t row, uint8_t col) {
    if (row >= MATRIX_HEIGHT || col >= MATRIX_WIDTH) return false;
    return frame_buffer[row][col];
}

void matrix_display_update(void) {
    if (text_len == 0) return;
    int max_offset = text_len * CHAR_TOTAL_WIDTH;
    if (scroll_enabled) {
        scroll_offset++;
        if (scroll_offset >= max_offset) {
            scroll_offset = INITIAL_OFFSET;
            // If test mode was active, restore normal text after one full cycle
            if (test_mode_active) {
                test_mode_active = false;
                if (saved_normal_len > 0) {
                    strncpy(text, saved_normal_text, sizeof(text)-1);
                    text[sizeof(text)-1] = '\0';
                    text_len = saved_normal_len;
                }
            }
        }
    }
    render_frame();
}

void matrix_display_set_text(const char *str) {
    if (text_mutex) {
        xSemaphoreTake(text_mutex, portMAX_DELAY);
        strncpy(text, str, sizeof(text)-1);
        text[sizeof(text)-1] = '\0';
        text_len = strlen(text);
        xSemaphoreGive(text_mutex);
    } else {
        strncpy(text, str, sizeof(text)-1);
        text[sizeof(text)-1] = '\0';
        text_len = strlen(text);
    }
    if (text_len > 0) {
        scroll_enabled = true;
        scroll_offset = INITIAL_OFFSET;
    }
    render_frame();
}

void matrix_display_set_text_no_reset(const char *str) {
    if (text_mutex) {
        xSemaphoreTake(text_mutex, portMAX_DELAY);
        strncpy(text, str, sizeof(text)-1);
        text[sizeof(text)-1] = '\0';
        text_len = strlen(text);
        xSemaphoreGive(text_mutex);
    } else {
        strncpy(text, str, sizeof(text)-1);
        text[sizeof(text)-1] = '\0';
        text_len = strlen(text);
    }
    render_frame();
}

void matrix_display_enable_scroll(bool enable) {
    scroll_enabled = enable;
}

bool matrix_display_is_scroll_enabled(void) {
    return scroll_enabled;
}

void matrix_display_save_state(void) {
    saved_scroll_offset = scroll_offset;
    saved_scroll_enabled = scroll_enabled;
}

void matrix_display_restore_state(void) {
    scroll_offset = saved_scroll_offset;
    scroll_enabled = saved_scroll_enabled;
    render_frame();
}

void matrix_display_set_offset(int offset) {
    scroll_offset = offset;
    render_frame();
}

void matrix_display_start_test(const char *test_text) {
    if (text_mutex) {
        xSemaphoreTake(text_mutex, portMAX_DELAY);
        // Save current normal text
        strncpy(saved_normal_text, text, sizeof(saved_normal_text)-1);
        saved_normal_text[sizeof(saved_normal_text)-1] = '\0';
        saved_normal_len = text_len;
        // Set test text
        strncpy(text, test_text, sizeof(text)-1);
        text[sizeof(text)-1] = '\0';
        text_len = strlen(text);
        test_mode_active = true;
        scroll_enabled = true;
        scroll_offset = INITIAL_OFFSET;
        xSemaphoreGive(text_mutex);
    }
    render_frame();
}

bool matrix_display_is_test_active(void) {
    return test_mode_active;
}

void matrix_display_stop_test(void) {
    if (test_mode_active) {
        test_mode_active = false;
        // Force restore
        if (saved_normal_len > 0) {
            strncpy(text, saved_normal_text, sizeof(text)-1);
            text[sizeof(text)-1] = '\0';
            text_len = saved_normal_len;
            scroll_offset = INITIAL_OFFSET;
            render_frame();
        }
    }
}