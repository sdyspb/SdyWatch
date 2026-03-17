#include <stdio.h>
#include <sys/time.h>
#include <clock_rtc.h>
#include <config.h>

void clock_rtc_init(void) {
    struct tm tm = {
        .tm_sec  = INITIAL_SECOND,
        .tm_min  = INITIAL_MINUTE,
        .tm_hour = INITIAL_HOUR,
        .tm_mday = 1,
        .tm_mon  = 0,
        .tm_year = 2024 - 1900
    };
    time_t t = mktime(&tm);
    struct timeval now = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&now, NULL);
}