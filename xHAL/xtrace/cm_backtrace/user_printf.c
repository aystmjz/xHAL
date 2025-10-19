
#include "xhal_config.h"
#include <stdarg.h>
#include <stdio.h>
#include XHAL_CMSIS_DEVICE_HEADER

void cmb_puts(const char *s)
{
    while (*s)
    {
        /* user code  */
        while ((USART1->SR & 0X40) == 0)
        {
        }
        USART1->DR = (u8)*s;
        /* user code end  */
        s++;
    }
}

void cmb_printf(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    cmb_puts(buf);
}