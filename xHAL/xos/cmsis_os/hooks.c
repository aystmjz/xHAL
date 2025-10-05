#include "../FreeRTOS/include/FreeRTOS.h"
#include "../FreeRTOS/include/event_groups.h"
#include "../FreeRTOS/include/semphr.h"
#include "../FreeRTOS/include/task.h"
#include "../xcore/xhal_assert.h"
#include "../xcore/xhal_log.h"
#include "../xcore/xhal_malloc.h"
#include "../xhal_os.h"

XLOG_TAG("xRTOS");

/**
 * @brief FreeRTOS 栈溢出钩子函数
 * @note  当任务检测到栈溢出时会调用该函数
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName)
{
    // 输出错误日志，打印任务名
    if (pcTaskName == NULL)
    {
        XLOG_ERROR("Stack overflow");
    }
    else
    {
        XLOG_ERROR("Stack overflow detected in task: %s",
                   *pcTaskName == 0 ? "NULL" : (char *)pcTaskName);
    }

    // 停机处理，防止系统继续跑崩掉
    taskDISABLE_INTERRUPTS();
    while (1)
    {
    }
}

/**
 * @brief FreeRTOS 内存分配失败钩子函数
 * @note  当 malloc (pvPortMalloc) 失败时被调用
 */
void vApplicationMallocFailedHook(void)
{
    uint16_t perused = xmem_perused();

    XLOG_ERROR("Memory allocation failed! Memory usage: %d.%d%%", perused / 10,
               perused % 10);

    // 停机处理，防止系统继续跑崩掉
    taskDISABLE_INTERRUPTS();
    while (1)
    {
    }
}