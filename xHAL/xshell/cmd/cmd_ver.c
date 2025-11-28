#include "../../xcore/xhal_common.h"
#include "../xhal_shell.h"
#include "cmd_config.h"

#define CMD_VER_DESCRIPTION "ver: display system version information\r\n"

#if SHELL_CMD_IS_ENABLED(VER)

static int ver_cmd(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    SHELL_ASSERT(shell, return -1);

    if (argc > 1)
    {
        shellPrint(shell, "usage: ver\r\n");
        shellPrint(shell, CMD_VER_DESCRIPTION);
        return -1;
    }

    shellPrint(shell, "%s", xhal_logo);

    shellPrint(shell, "\r\n");
    shellPrint(shell, "Version: %s (0x%08X)\r\n", xhal_version_str(),
               xhal_version());

    shellPrint(shell, "Build Date: %s\r\n", XHAL_BUILD_DATE);
    shellPrint(shell, "Build Time: %s\r\n", XHAL_BUILD_TIME);

    shellPrint(shell, "\r\n");
    return 0;
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), ver, ver_cmd,
    "\r\ndisplay system version information\r\n" CMD_VER_DESCRIPTION);

#endif /* SHELL_CMD_IS_ENABLED(VER) */
