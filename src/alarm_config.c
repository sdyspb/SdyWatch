#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "alarm_config.h"

alarm_config_t alarm_config;

// Значения по умолчанию
void alarm_config_init(void) {
    printf("alarm_config_init() called\n");
        
    for (int i = 0; i < MAX_ALARMS; i++) {
        alarm_config.alarms[i].hour = 7;
        alarm_config.alarms[i].minute = 0;
        alarm_config.alarms[i].enabled = false;
        snprintf(alarm_config.alarms[i].message, ALARM_MSG_LEN, "Alarm %d", i+1);
    }
}

// ---------- Команда alarm show ----------
static struct {
    struct arg_end *end;
} show_args;

static int show_cmd(int argc, char **argv) {
    printf("\n=== Alarm Configuration ===\n");
    for (int i = 0; i < MAX_ALARMS; i++) {
        alarm_t *a = &alarm_config.alarms[i];
        printf("Alarm %d: %s %02d:%02d \"%s\"\n",
               i+1,
               a->enabled ? "ENABLED " : "DISABLED",
               a->hour, a->minute,
               a->message);
    }
    return 0;
}

static void register_show(void) {
    show_args.end = arg_end(1);
    const esp_console_cmd_t cmd = {
        .command = "alarm_show",
        .help = "Show all alarm settings",
        .hint = NULL,
        .func = &show_cmd,
        .argtable = &show_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// ---------- Команда alarm set ----------
static struct {
    struct arg_int *index;
    struct arg_int *hour;
    struct arg_int *minute;
    struct arg_str *message;
    struct arg_end *end;
} set_args;

static int set_cmd(int argc, char **argv) {
    int nerrors = arg_parse(argc, argv, (void**) &set_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, set_args.end, argv[0]);
        return 1;
    }

    int idx = set_args.index->ival[0] - 1; // 1-based to 0-based
    if (idx < 0 || idx >= MAX_ALARMS) {
        printf("Error: index must be 1..%d\n", MAX_ALARMS);
        return 1;
    }

    int hour = set_args.hour->ival[0];
    int minute = set_args.minute->ival[0];
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        printf("Error: invalid time\n");
        return 1;
    }

    alarm_config.alarms[idx].hour = hour;
    alarm_config.alarms[idx].minute = minute;

    if (set_args.message->count > 0) {
        strncpy(alarm_config.alarms[idx].message, set_args.message->sval[0], ALARM_MSG_LEN - 1);
        alarm_config.alarms[idx].message[ALARM_MSG_LEN - 1] = '\0';
    }

    printf("Alarm %d set to %02d:%02d message: %s\n",
           idx+1, hour, minute, alarm_config.alarms[idx].message);
    return 0;
}

static void register_set(void) {
    set_args.index = arg_int1(NULL, NULL, "<1..3>", "alarm index");
    set_args.hour = arg_int1(NULL, NULL, "<hour>", "hour (0-23)");
    set_args.minute = arg_int1(NULL, NULL, "<minute>", "minute (0-59)");
    set_args.message = arg_str0(NULL, NULL, "<message>", "optional alarm message");
    set_args.end = arg_end(4);

    const esp_console_cmd_t cmd = {
        .command = "alarm_set",
        .help = "Set alarm time and optional message",
        .hint = NULL,
        .func = &set_cmd,
        .argtable = &set_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// ---------- Команда alarm enable ----------
static struct {
    struct arg_int *index;
    struct arg_int *enable;
    struct arg_end *end;
} enable_args;

static int enable_cmd(int argc, char **argv) {
    int nerrors = arg_parse(argc, argv, (void**) &enable_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, enable_args.end, argv[0]);
        return 1;
    }

    int idx = enable_args.index->ival[0] - 1;
    if (idx < 0 || idx >= MAX_ALARMS) {
        printf("Error: index must be 1..%d\n", MAX_ALARMS);
        return 1;
    }

    int en = enable_args.enable->ival[0];
    if (en != 0 && en != 1) {
        printf("Error: enable must be 0 or 1\n");
        return 1;
    }

    alarm_config.alarms[idx].enabled = (en == 1);
    printf("Alarm %d %s\n", idx+1, alarm_config.alarms[idx].enabled ? "enabled" : "disabled");
    return 0;
}

static void register_enable(void) {
    enable_args.index = arg_int1(NULL, NULL, "<1..3>", "alarm index");
    enable_args.enable = arg_int1(NULL, NULL, "<0/1>", "enable (1) or disable (0)");
    enable_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "alarm_enable",
        .help = "Enable or disable an alarm",
        .hint = NULL,
        .func = &enable_cmd,
        .argtable = &enable_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// Регистрация всех команд (без help, т.к. REPL предоставляет встроенную help)
void register_alarm_commands(void) {
    register_show();
    register_set();
    register_enable();
}