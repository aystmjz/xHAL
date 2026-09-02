/**
 ******************************************************************************
 * @file    xhal_assert.c
 * @author  aystmjz
 * @brief   断言模块源文件，实现断言失败信息输出及断言失败处理函数
 * @version 2.3.0
 * @date    2026-08-23
 ******************************************************************************
 * Copyright (c) 2026 aystmjz. All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "xhal_assert.h"
#include <stdio.h>
#include XHAL_DEVICE_HEADER

#if (XHAL_OS_SUPPORTING == 1)
    #include "../xos/xhal_os.h"
#endif /* (XHAL_OS_SUPPORTING == 1) */

/* Private functions ---------------------------------------------------------*/
#if (XASSERT_USER_HOOK_ENABLE != 0)
extern void xassert_user_hook(void);
/**
 * @brief  断言失败用户钩子函数（弱定义）
 * @note   用户可在应用程序中重定义该函数，断言失败时先执行此函数
 */
XHAL_WEAK void xassert_user_hook(void)
{
    /* ----------- user code start --------------- */
    /* do nothing */
    /* -----------  user code end  --------------- */
}
#endif /* (XASSERT_USER_HOOK_ENABLE != 0) */

/* Private functions ---------------------------------------------------------*/
/**
 * @brief  断言失败后的最终处理函数
 * @note   先调用用户钩子 xassert_user_hook()，然后关闭全局中断并进入死循环
 */
void _xassert_func(void)
{
#if (XASSERT_USER_HOOK_ENABLE != 0)
    xassert_user_hook();
#endif /* (XASSERT_USER_HOOK_ENABLE != 0) */

    XHAL_DISABLE_IRQ();

    while (1)
    {
    }
}

/**
 * @brief  断言失败信息输出函数
 * @note   通过 xhal_emerg_puts() 输出断言失败的条件、模块、位置等信息，
 *         支持 OS 时调用 osKernelLock() 锁定内核
 * @param  condition: 失败的条件表达式
 * @param  extra: 附加说明信息，可为 NULL
 * @param  tag: 模块标签
 * @param  file: 断言所在文件名
 * @param  func: 断言所在函数名
 * @param  line: 断言所在行号
 * @param  id: 断言 ID
 */
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
#endif /* (XHAL_OS_SUPPORTING == 1) */
}

/* ---------------------------------------------------------------------------*/