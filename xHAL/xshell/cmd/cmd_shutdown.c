#include "../../xcore/xhal_export.h"
#include "../xhal_shell.h"
#include "cmd_config.h"
#include XHAL_DEVICE_HEADER

#define POWER_ENTER_STANDBY()    PWR_EnterSTANDBYMode()

#define CMD_SHUTDOWN_DESCRIPTION "shutdown: safely shutdown the system\r\n"

#if SHELL_CMD_IS_ENABLED(SHUTDOWN)

static int shutdown_cmd(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    SHELL_ASSERT(shell, return -1);

    if (argc > 1)
    {
        shellPrint(shell, "usage:\r\n");
        shellPrint(shell, CMD_SHUTDOWN_DESCRIPTION);
        return -1;
    }

    shellPrint(shell, "System is shutting down...\r\n");
    xhal_exit();
    
    POWER_ENTER_STANDBY();

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 shutdown, shutdown_cmd,
                 "\r\nSafely shutdown the system\r\n" CMD_SHUTDOWN_DESCRIPTION);

#endif /* SHELL_CMD_IS_ENABLED(SHUTDOWN) */
