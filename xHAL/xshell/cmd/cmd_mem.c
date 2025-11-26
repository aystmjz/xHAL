#include "../../xcore/xhal_common.h"
#include "../../xcore/xhal_malloc.h"
#include "../xhal_shell.h"
#include "cmd_config.h"
#include <stdlib.h>
#include <string.h>

#define CMD_MEMINFO_DESCRIPTION "mem\r\n"

#if SHELL_CMD_IS_ENABLED(MEM)
static int meminfo_cmd(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    SHELL_ASSERT(shell, return -1);

    if (argc != 1)
    {
        shellPrint(shell, "usage:\r\n\r\n" CMD_MEMINFO_DESCRIPTION);
        return -1;
    }

    uint32_t free_size = xmem_free_size();
    uint16_t perused   = xmem_perused();

    shellPrint(shell, "\r\n[ Memory Info ]\r\n");
    shellPrint(shell, "  FreeSize    : %lu bytes\r\n",
               (unsigned long)free_size);
    shellPrint(shell, "  UsedPercent : %d.%d%%\r\n", perused / 10,
               perused % 10);

    shellPrint(shell, "  Usage Bar   : [");
    for (int i = 0; i < 50; i++)
    {
        if (i < perused / 20)
            shellPrint(shell, "#");
        else
            shellPrint(shell, ".");
    }
    shellPrint(shell, "]\r\n\r\n");

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 mem, meminfo_cmd,
                 "\r\nshow memory usage\r\n" CMD_MEMINFO_DESCRIPTION);
#endif