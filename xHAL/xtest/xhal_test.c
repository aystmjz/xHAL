#include "xhal_test.h"
#include "..\xshell\xhal_shell.h"

#if (XHAL_SHELL == 1)
    #include "..\xcore\xhal_export.h"
#endif

void unity_putc(char c)
{
#if (XHAL_SHELL == 1)
    Shell *shell = shellGetCurrent();
    shell->write(&c, 1);
#endif
}

#if (XHAL_SHELL == 1) && (XHAL_UNIT_TEST == 1)
static int unity_cmd(int argc, const char *argv[])
{
    return UnityMain(argc, argv, &xhal_unit_test);
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 unity, unity_cmd, run unity test);
#endif