#include "../../xcore/xhal_export.h"
#include "../xhal_shell.h"
#include "cmd_config.h"
#include XHAL_DEVICE_HEADER

#define POWER_RESET()          NVIC_SystemReset()

#define CMD_REBOOT_DESCRIPTION "reboot: safely reboot the system\r\n"

#if SHELL_CMD_IS_ENABLED(REBOOT)

static int reboot_cmd(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    SHELL_ASSERT(shell, return -1);

    if (argc > 1)
    {
        shellPrint(shell, "usage: reboot\r\n");
        shellPrint(shell, CMD_REBOOT_DESCRIPTION);
        return -1;
    }

    shellPrint(shell, "System is rebooting...\r\n");
    xhal_exit();

    POWER_RESET();

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 reboot, reboot_cmd,
                 "\r\nSafely reboot the system\r\n" CMD_REBOOT_DESCRIPTION);

#endif /* SHELL_CMD_IS_ENABLED(REBOOT) */
