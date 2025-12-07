#include "xhal_assert.h"
#include "xhal_log.h"
#include "xhal_time.h"
#include <stdio.h>

XLOG_TAG("xAssert");

#ifdef XHAL_OS_SUPPORTING
#include "../xos/xhal_os.h"
#endif

#define XASSERT_DISABLE_IRQ() __disable_irq()

#ifndef XASSERT_USER_HOOK
#define XASSERT_USER_HOOK (1)
#endif

#if XASSERT_USER_HOOK_ENABLE != 0
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

    XASSERT_DISABLE_IRQ();

    while (1)
    {
    }
}

void _xassert(const char *condition, const char *extra, const char *tag,
              const char *file, const char *func, uint32_t line, uint32_t id)
{
    if (condition == NULL || tag == NULL || file == NULL)
        return;

    if (id == XASSERT_INVALID_ID)
        XLOG_ERROR("\r\n\r\n==============================\r\n"
                   " Assert failure!\r\n"
                   " Condition| %s\r\n"
                   " Module   | %s\r\n"
                   " Location | %s:%d\r\n"
                   " Function | %s\r\n"
                   " Info     | %s\r\n"
                   "==============================",
                   condition, tag, file, line, func,
                   extra == NULL ? "<none>" : extra);
    else
        XLOG_ERROR("\r\n\r\n==============================\r\n"
                   " Assert failure!\r\n"
                   " Condition| %s\r\n"
                   " Module   | %s\r\n"
                   " Location | %s:%d\r\n"
                   " Function | %s\r\n"
                   " ID       | %d\r\n"
                   " Info     | %s\r\n"
                   "==============================",
                   condition, tag, file, line, func, id,
                   extra == NULL ? "<none>" : extra);

    xtime_delay_ms(50); /* 确保串口输出完毕 */

#ifdef XHAL_OS_SUPPORTING
    osKernelLock();
#endif
}
