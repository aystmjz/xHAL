#include "../../xcore/xhal_common.h"
#include "../../xcore/xhal_time.h"
#include "../xhal_shell.h"
#include "cmd_config.h"
#include <stdlib.h>
#include <string.h>

#define CMD_TIME_CDISCRIPTION             \
    "time [-u | -c | -s <timestamp>]\r\n" \
    " -u: display uptime\r\n"             \
    " -c: display current time\r\n"       \
    " -s: set current time with timestamp (Unix timestamp)\r\n"

#if SHELL_CMD_IS_ENABLED(TIME)
static int time_cmd(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    SHELL_ASSERT(shell, return -1);

    if (argc > 3)
    {
        shellPrint(shell, "usage:\r\n");
        shellPrint(shell, CMD_TIME_CDISCRIPTION);
        return -1;
    }

    uint8_t show_uptime       = 1;
    uint8_t show_current_time = 1;
    uint8_t set_time          = 0;
    xhal_ts_t timestamp       = 0;

    if (argc == 1)
    {
        /* 无参数：显示两者 */
    }
    else if (argc == 2)
    {
        /* 一个参数：-u 或 -c */
        if (strcmp(argv[1], "-u") == 0)
        {
            show_current_time = 0; /* 只显示 uptime */
        }
        else if (strcmp(argv[1], "-c") == 0)
        {
            show_uptime = 0; /* 只显示当前时间 */
        }
        else
        {
            shellPrint(shell, "unknown parameter: %s\r\n", argv[1]);
            shellPrint(shell, "usage:\r\n");
            shellPrint(shell, CMD_TIME_CDISCRIPTION);
            return -1;
        }
    }
    else if (argc == 3)
    {
        /* 两个参数：-s 和时间戳 */
        if (strcmp(argv[1], "-s") == 0)
        {
            set_time  = 1;
            timestamp = (xhal_ts_t)strtoul(argv[2], NULL, 0);
        }
        else
        {
            shellPrint(shell, "unknown parameter: %s\r\n", argv[1]);
            shellPrint(shell, "usage:\r\n");
            shellPrint(shell, CMD_TIME_CDISCRIPTION);
            return -1;
        }
    }

    char time_buf[32];
    xhal_err_t ret;

    if (set_time)
    {
        shellPrint(shell, "Setting time to timestamp: %lu\r\n",
                   (unsigned long)timestamp);

        ret = xtime_sync_time(timestamp);
        if (ret == XHAL_OK)
        {
            shellPrint(shell, "Time set successfully\r\n");

            /* 显示设置后的时间 */
            ret = xtime_get_format_time(time_buf, sizeof(time_buf));
            if (ret == XHAL_OK)
            {
                shellPrint(shell, "New current time: %s\r\n", time_buf);
            }
            else
            {
                shellPrint(shell, "Get new time failed: %s\r\n",
                           xhal_err_to_str(ret));
                return -1;
            }
        }
        else
        {
            shellPrint(shell, "Set time failed: %s\r\n", xhal_err_to_str(ret));
            return -1;
        }
        return 0;
    }

    uint8_t error = 0;

    /* 处理显示时间 */
    if (show_uptime)
    {
        ret = xtime_get_format_uptime(time_buf, sizeof(time_buf));
        if (ret == XHAL_OK)
        {
            shellPrint(shell, "Uptime: %s\r\n", time_buf);
        }
        else
        {
            shellPrint(shell, "Get uptime failed: %s\r\n",
                       xhal_err_to_str(ret));
            error = 1;
        }
    }

    if (show_current_time)
    {
        ret = xtime_get_format_time(time_buf, sizeof(time_buf));
        if (ret == XHAL_OK)
        {
            shellPrint(shell, "Current time: %s\r\n", time_buf);
        }
        else
        {
            shellPrint(shell, "Get current time failed: %s\r\n",
                       xhal_err_to_str(ret));
            error = 1;
        }
    }

    if (error)
    {
        return -1;
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 time, time_cmd, "\r\ntime command\r\n" CMD_TIME_CDISCRIPTION);
#endif