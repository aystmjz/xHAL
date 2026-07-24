#include "../../xcore/xhal_common.h"
#include "../../xcore/xhal_coro.h"
#include "../xhal_shell.h"
#include "cmd_config.h"

#define CMD_USAGE_DESCRIPTION "usage: display current CPU usage percentage\r\n"

#if SHELL_CMD_IS_ENABLED(USAGE)
static int usage_cmd(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    SHELL_ASSERT(shell, return -1);

    if (argc != 1)
    {
        shellPrint(shell, "usage:\r\n");
        shellPrint(shell, CMD_USAGE_DESCRIPTION);
        return -1;
    }

    uint16_t cpu_usage = xcoro_cpu_usage_get();

    shellPrint(shell, "\r\n[ CPU Usage ]\r\n");
    shellPrint(shell, "  UsedPercent : %u.%02u%%\r\n", cpu_usage / 100,
               cpu_usage % 100);

    shellPrint(shell, "  Usage Bar   : [");
    for (uint8_t i = 0; i < 50; i++)
    {
        if (i < cpu_usage / 200)
            shellPrint(shell, "#");
        else
            shellPrint(shell, ".");
    }
    shellPrint(shell, "]\r\n\r\n");

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 usage, usage_cmd,
                 "\r\ndisplay current CPU usage\r\n" CMD_USAGE_DESCRIPTION);

#endif /* SHELL_CMD_IS_ENABLED(USAGE) */
