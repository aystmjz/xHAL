#include "../../xcore/xhal_export.h"
#include "../xhal_shell.h"
#include "cmd_config.h"
#include XHAL_DEVICE_HEADER
#include <string.h>

#define CMD_REBOOT_DESCRIPTION "reboot\r\nsafely reboot the system\r\n"

#if SHELL_CMD_IS_ENABLED(REBOOT)

static int reboot_cmd(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    SHELL_ASSERT(shell, return -1);

    if (argc > 1)
    {
        if (strcmp(argv[1], "-h") == 0)
        {
            shellPrint(shell, "%s", CMD_REBOOT_DESCRIPTION);
            return 0;
        }
        shellPrint(shell, "usage: %s", CMD_REBOOT_DESCRIPTION);
        return -1;
    }

    shellPrint(shell, "System is rebooting...\r\n");
    xhal_exit();

    XHAL_POWER_RESET();

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 reboot, reboot_cmd, safely reboot the system);

#endif /* SHELL_CMD_IS_ENABLED(REBOOT) */
