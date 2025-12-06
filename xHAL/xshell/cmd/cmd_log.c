#include "../../xcore/xhal_common.h"
#include "../../xcore/xhal_log.h"
#include "../xhal_shell.h"
#include "cmd_config.h"
#include <stdlib.h>
#include <string.h>

#define CMD_LOG_USAGE                              \
    "log [-s | -g | -l | -m | -t]\r\n"             \
    " -s <level>: set log level (0-4 or name)\r\n" \
    " -m <mode>: set time display mode\r\n"        \
    " -g: get current log level\r\n"               \
    " -l: list available log levels\r\n"           \
    " -t: get current time mode\r\n"

#define CMD_LOG_OPTIONS                                       \
    "Levels: 0=none, 1=error, 2=warning, 3=info, 4=debug\r\n" \
    "Time modes: 0=none, 1=millis, 2=relative, 3=absolute\r\n"

#if SHELL_CMD_IS_ENABLED(LOG)
static const char *log_level_names[] = {
    "none",    /* 0 */
    "error",   /* 1 */
    "warning", /* 2 */
    "info",    /* 3 */
    "debug"    /* 4 */
};

static const char *log_time_mod_names[] = {
    "none",     /* 0 */
    "millis",   /* 1 */
    "relative", /* 2 */
    "absolute"  /* 3 */
};

static int8_t parse_log_level(const char *level_str, uint8_t *level)
{
    /* Try to parse as number */
    char *endptr;
    long num = strtol(level_str, &endptr, 10);

    if (endptr != level_str && *endptr == '\0')
    {
        /* Valid number */
        if (num >= 0 && num < XLOG_LEVEL_MAX)
        {
            *level = (uint8_t)num;
            return 0;
        }
        return -1; /* Out of range */
    }

    /* Try to parse as name */
    for (uint8_t i = 0; i < XLOG_LEVEL_MAX; i++)
    {
        if (strcasecmp(level_str, log_level_names[i]) == 0)
        {
            *level = i;
            return 0;
        }
    }

    return -1; /* Not found */
}

static int8_t parse_time_mode(const char *mode_str, uint8_t *mode)
{
    /* Try to parse as number */
    char *endptr;
    long num = strtol(mode_str, &endptr, 10);

    if (endptr != mode_str && *endptr == '\0')
    {
        /* Valid number */
        if (num >= 0 && num < XLOG_TIME_MOD_MAX)
        {
            *mode = (uint8_t)num;
            return 0;
        }
        return -1; /* Out of range */
    }

    /* Try to parse as name */
    for (uint8_t i = 0; i < XLOG_TIME_MOD_MAX; i++)
    {
        if (strcasecmp(mode_str, log_time_mod_names[i]) == 0)
        {
            *mode = i;
            return 0;
        }
    }

    return -1; /* Not found */
}

static const char *get_log_level_str(uint8_t level)
{
    if (level < XLOG_LEVEL_MAX)
    {
        return log_level_names[level];
    }
    return "unknown";
}

static const char *get_time_mode_str(uint8_t mode)
{
    if (mode < XLOG_TIME_MOD_MAX)
    {
        return log_time_mod_names[mode];
    }
    return "unknown";
}
static int log_cmd(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    SHELL_ASSERT(shell, return -1);

    if (argc > 3)
    {
        shellPrint(shell, "usage:\r\n");
        shellPrint(shell, CMD_LOG_USAGE);
        shellPrint(shell, CMD_LOG_OPTIONS);
        return -1;
    }

    uint8_t set_level    = 0;
    uint8_t get_level    = 0;
    uint8_t list_levels  = 0;
    uint8_t set_time_mod = 0;
    uint8_t get_time_mod = 0;
    uint8_t new_level    = 0;
    uint8_t new_time_mod = 0;

    if (argc == 1)
    {
        get_level    = 1;
        get_time_mod = 1;
    }
    else if (argc == 2)
    {
        if (strcmp(argv[1], "-g") == 0)
        {
            get_level = 1;
        }
        else if (strcmp(argv[1], "-l") == 0)
        {
            list_levels = 1;
        }
        else if (strcmp(argv[1], "-t") == 0)
        {
            get_time_mod = 1;
        }
        else
        {
            shellPrint(shell, "unknown parameter: %s\r\n", argv[1]);
            shellPrint(shell, "usage:\r\n");
            shellPrint(shell, CMD_LOG_USAGE);
            shellPrint(shell, CMD_LOG_OPTIONS);
            return -1;
        }
    }
    else if (argc == 3)
    {
        if (strcmp(argv[1], "-s") == 0)
        {
            set_level = 1;
            if (parse_log_level(argv[2], &new_level) != 0)
            {
                shellPrint(shell, "invalid log level: %s\r\n", argv[2]);
                shellPrint(
                    shell,
                    "valid levels: 0-4 or none,error,warning,info,debug\r\n");
                return -1;
            }
        }
        else if (strcmp(argv[1], "-m") == 0)
        {
            set_time_mod = 1;
            if (parse_time_mode(argv[2], &new_time_mod) != 0)
            {
                shellPrint(shell, "invalid time mode: %s\r\n", argv[2]);
                shellPrint(
                    shell,
                    "valid modes: 0-3 or none,millis,relative,absolute\r\n");
                return -1;
            }
        }
        else
        {
            shellPrint(shell, "unknown parameter: %s\r\n", argv[1]);
            shellPrint(shell, "usage:\r\n");
            shellPrint(shell, CMD_LOG_USAGE);
            shellPrint(shell, CMD_LOG_OPTIONS);
            return -1;
        }
    }

    if (list_levels)
    {
        shellPrint(shell, "Available log levels:\r\n");
        shellPrint(shell, "----------------------------------------\r\n");
        shellPrint(shell, "Level | Name    | Description\r\n");
        shellPrint(shell, "------|---------|------------------------\r\n");
        shellPrint(shell, "  %d   | %-7s | %s\r\n", 0, log_level_names[0],
                   "No logs");
#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_ERROR
        shellPrint(shell, "  %d   | %-7s | %s\r\n", 1, log_level_names[1],
                   "Errors only");
#endif
#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_WARNING
        shellPrint(shell, "  %d   | %-7s | %s\r\n", 2, log_level_names[2],
                   "Warnings + Errors");
#endif
#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_INFO
        shellPrint(shell, "  %d   | %-7s | %s\r\n", 3, log_level_names[3],
                   "Info + Warnings + Errors");
#endif
#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_DEBUG
        shellPrint(shell, "  %d   | %-7s | %s\r\n", 4, log_level_names[4],
                   "All logs (Debug)");
#endif
        shellPrint(shell, "----------------------------------------\r\n");

        shellPrint(shell, "\r\nAvailable time display modes:\r\n");
        shellPrint(shell, "----------------------------------------\r\n");
        shellPrint(shell, "Mode | Name     | Description\r\n");
        shellPrint(shell, "-----|----------|------------------------\r\n");
        shellPrint(shell, "  %d  | %-8s | %s\r\n", 0, log_time_mod_names[0],
                   "No time display");
        shellPrint(shell, "  %d  | %-8s | %s\r\n", 1, log_time_mod_names[1],
                   "Milliseconds timestamp");
        shellPrint(shell, "  %d  | %-8s | %s\r\n", 2, log_time_mod_names[2],
                   "Relative time (HH:MM:SS.XXX)");
        shellPrint(shell, "  %d  | %-8s | %s\r\n", 3, log_time_mod_names[3],
                   "Absolute time (YYYY-MM-DD HH:MM:SS)");
        shellPrint(shell, "----------------------------------------\r\n");
        return 0;
    }

    if (get_level)
    {
        uint8_t current_level = xlog_get_level();
        shellPrint(shell, "Current log level: %d (%s)\r\n", current_level,
                   get_log_level_str(current_level));
    }

    if (get_time_mod)
    {
        uint8_t current_time = xlog_get_time_mod();
        shellPrint(shell, "Current time mode: %d (%s)\r\n", current_time,
                   get_time_mode_str(current_time));
    }

    if (set_level)
    {
        uint8_t old_level = xlog_get_level();

        if (new_level > XLOG_COMPILE_LEVEL)
        {
            shellPrint(shell, "Error: Cannot set level to %d (%s)\r\n",
                       new_level, get_log_level_str(new_level));
            shellPrint(shell, "Only levels 0-%d are available\r\n",
                       XLOG_COMPILE_LEVEL);
            return -1;
        }

        shellPrint(shell, "Setting log level from %d (%s) to %d (%s)\r\n",
                   old_level, get_log_level_str(old_level), new_level,
                   get_log_level_str(new_level));

        xhal_err_t ret = xlog_set_level(new_level);
        if (ret != XHAL_OK)
        {
            shellPrint(shell, "Failed to set log level: %s\r\n",
                       xhal_err_to_str(ret));
            return -1;
        }

        shellPrint(shell, "Log level set successfully\r\n");
    }

    if (set_time_mod)
    {
        uint8_t old_time_mod = xlog_get_time_mod();

        shellPrint(shell, "Setting time mode from %d (%s) to %d (%s)\r\n",
                   old_time_mod, get_time_mode_str(old_time_mod), new_time_mod,
                   get_time_mode_str(new_time_mod));

        xhal_err_t ret = xlog_set_time_mod(new_time_mod);
        if (ret != XHAL_OK)
        {
            shellPrint(shell, "Failed to set time mode: %s\r\n",
                       xhal_err_to_str(ret));
            return -1;
        }

        shellPrint(shell, "Time mode set successfully\r\n");

        /* 显示示例 */
        shellPrint(shell, "\r\nExample format:\r\n");
        switch (new_time_mod)
        {
        case XLOG_TIME_MOD_NONE:
            shellPrint(shell, "  [I/LOG] This is an info message\r\n");
            break;
        case XLOG_TIME_MOD_MILLIS:
            shellPrint(shell, "  [I/LOG 0123ms] This is an info message\r\n");
            break;
        case XLOG_TIME_MOD_RELATIVE:
            shellPrint(shell,
                       "  [I/LOG 00:01:23.456] This is an info message\r\n");
            break;
        case XLOG_TIME_MOD_ABSOLUTE:
            shellPrint(
                shell,
                "  [I/LOG 2024-01-01 12:34:56] This is an info message\r\n");
            break;
        }
    }
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 log, log_cmd,
                 "\r\nset/get log levels\r\n" CMD_LOG_USAGE CMD_LOG_OPTIONS);

#endif /* SHELL_CMD_IS_ENABLED(LOG) */
