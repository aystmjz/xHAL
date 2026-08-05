#include "../../xcore/xhal_export.h"
#include "../xhal_shell.h"
#include "cmd_config.h"
#include <string.h>

#define CMD_SHUTDOWN_DESCRIPTION "shutdown\r\nsafely shutdown the system\r\n"

#if SHELL_CMD_IS_ENABLED(SHUTDOWN)

static int shutdown_cmd(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    SHELL_ASSERT(shell, return -1);

    if (argc > 1)
    {
        if (strcmp(argv[1], "-h") == 0)
        {
            shellPrint(shell, "%s", CMD_SHUTDOWN_DESCRIPTION);
            return 0;
        }
        shellPrint(shell, "usage: %s", CMD_SHUTDOWN_DESCRIPTION);
        return -1;
    }

    shellPrint(shell, "System is shutting down...\r\n");

    xhal_exit();

    while (1)
    {
    }
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 shutdown, shutdown_cmd, safely shutdown the system);

#endif /* SHELL_CMD_IS_ENABLED(SHUTDOWN) */
