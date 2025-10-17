#include "xhal_assert.h"
#include "xhal_log.h"
#include <stdio.h>

XLOG_TAG("xAssert");

void XHAL_WEAK xassert_func(void)
{
    while (1)
    {
    }
}

void _assert(const char *str, uint32_t id, const char *tag, const char *file,
             const char *func, uint32_t line)
{
    if (str == NULL || tag == NULL || file == NULL)
        return;

    if (id == (uint32_t)XASSERT_INVALID_ID)
        XLOG_ERROR("\r\n==============================\r\n"
                   " Assert failure!\r\n"
                   " Module   | %s\r\n"
                   " Location | %s:%d\r\n"
                   " Function | %s\r\n"
                   " Info     | %s\r\n"
                   "=============================\r",
                   tag, file, line, func, str);
    else
        XLOG_ERROR("\r\n==============================\r\n"
                   " Assert failure!\r\n"
                   " Module   | %s\r\n"
                   " Location | %s:%d\r\n"
                   " Function | %s\r\n"
                   " Info     | %s\r\n"
                   " ID       | %d\r\n"
                   "==============================",
                   tag, file, line, func, str, id);
    xassert_func();
}
