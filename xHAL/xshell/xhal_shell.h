#ifndef __XHAL_SHELL_H
#define __XHAL_SHELL_H

#include "xhal_config.h"

#if (XHAL_SHELL == 1)
    #include "Shell/shell.h"
#endif

/* User shell key */
#define ESH_KEY_CTRL_PLUS_A (0x01000000)

#endif /* __XHAL_SHELL_H */