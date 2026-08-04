#include "xhal_os.h"
#include "../xcore/xhal_assert.h"
#include "../xcore/xhal_log.h"
#include "../xcore/xhal_malloc.h"
#include "FreeRTOS/include/FreeRTOS.h"
#include "FreeRTOS/include/task.h"
#include <stdio.h>

XHAL_TAG(xOS);

/**
 * @brief FreeRTOS 栈溢出钩子函数
 * @note  当任务检测到栈溢出时会调用该函数
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    char info[configMAX_TASK_NAME_LEN + 42];

    snprintf(info, sizeof(info), "task \"%s\" stack overflow! tcb=0x%08X",
             *pcTaskName == '\0' ? "Unnamed" : (const char *)pcTaskName,
             (uint32_t)xTask);

    xassert_info(false, info);
}

/**
 * @brief FreeRTOS 内存分配失败钩子函数
 * @note  当 malloc (pvPortMalloc) 失败时被调用
 */
void vApplicationMallocFailedHook(void)
{
    uint16_t perused = xmem_perused();
    char info[20];

    snprintf(info, sizeof(info), "mem usage: %d.%d%%", perused / 10,
             perused % 10);

    xassert_info(false, info);
}