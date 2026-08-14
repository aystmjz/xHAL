#include "xhal_assert.h"
#include <stdio.h>
#include XHAL_DEVICE_HEADER

#if (XHAL_OS_SUPPORTING == 1)
    #include "../xos/xhal_os.h"
#endif

#ifndef XASSERT_USER_HOOK
    #define XASSERT_USER_HOOK 1
#endif

#if XASSERT_USER_HOOK_ENABLE != 0
extern void xassert_user_hook();
XHAL_WEAK void xassert_user_hook()
{
    /* ----------- user code start --------------- */
    /* do nothing */
    /* -----------  user code end  --------------- */
}
#endif

void _xassert_func(void)
{
#if XASSERT_USER_HOOK_ENABLE != 0
    xassert_user_hook();
#endif

    XHAL_DISABLE_IRQ();

    while (1)
    {
    }
}

void _xassert(const char *condition, const char *extra, const char *tag,
              const char *file, const char *func, uint32_t line, uint32_t id)
{
    char buf[8];

    if (condition == NULL || tag == NULL || file == NULL)
        return;

    xhal_emerg_puts("\r\n\r\n==============================\r\n"
                    " Assert failure!\r\n"
                    " Condition| ");
    xhal_emerg_puts(condition);
    xhal_emerg_puts("\r\n Module   | ");
    xhal_emerg_puts(tag);
    xhal_emerg_puts("\r\n Location | ");
    xhal_emerg_puts(file);
    xhal_emerg_puts(":");
    snprintf(buf, sizeof(buf), "%u", (unsigned int)line);
    xhal_emerg_puts(buf);
    xhal_emerg_puts("\r\n Function | ");
    xhal_emerg_puts(func);

    if (id != XASSERT_INVALID_ID)
    {
        xhal_emerg_puts("\r\n ID       | ");
        snprintf(buf, sizeof(buf), "%u", (unsigned int)id);
        xhal_emerg_puts(buf);
    }

    xhal_emerg_puts("\r\n Info     | ");
    xhal_emerg_puts(extra == NULL ? "<none>" : extra);
    xhal_emerg_puts("\r\n==============================\r\n");

#if (XHAL_OS_SUPPORTING == 1)
    osKernelLock();
#endif
}
