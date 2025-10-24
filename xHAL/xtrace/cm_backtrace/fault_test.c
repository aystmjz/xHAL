#include "../../xcore/xhal_log.h"
#include "../../xcore/xhal_time.h"
#include "../xhal_trace.h"
#include "cmb_cfg.h"
#include <stdio.h>
#include XHAL_CMSIS_DEVICE_HEADER

XLOG_TAG("xFaultTest");

/**
 * @brief  触发不同类型的硬件错误
 * @param  type: 错误类型
 */
void fault_trigger(uint8_t type)
{
    XLOG_INFO("Triggering fault type = %d", type);

    xtime_delay_ms(30);

    switch (type)
    {
    case FAULT_DIV0:
    {
        /* 打开除0陷阱 */
        SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;

        volatile int a = 10;
        volatile int b = 0;
        volatile int c = a / b; /* 除以0触发 UsageFault */
        (void)c;
        break;
    }

    case FAULT_UNALIGN:
    {
        /* 打开非对齐访问陷阱 */
        SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk;

        volatile int *p  = (int *)0x03; /* 非对齐地址 */
        volatile int val = *p;          /* 触发 UsageFault */
        (void)val;
        break;
    }

    case FAULT_INV_ADDR:
    {
        volatile int *p  = (int *)0xFFFFFF00; /* 无效地址 */
        volatile int val = *p; /* 触发 BusFault 或 HardFault */
        (void)val;
        break;
    }

    case FAULT_STACK_OVER:
    {
        /* 开大数组，模拟栈溢出 */
        uint8_t buf[2048] = {0};
        for (uint32_t i = 1; i < sizeof(buf); i++)
        {
            buf[i] = buf[i - 1] + 1;
        }
        break;
    }

    case FAULT_INV_EXEC:
    {
        /* 尝试执行 RAM 数据区内容 */
        typedef void (*func_t)(void);
        uint32_t *data_addr = (uint32_t *)0x20000000; /* RAM */
        func_t f            = (func_t)data_addr;
        f(); /* 执行数据区内容，触发 UsageFault */
        break;
    }

    default:
        XLOG_ERROR("Unknown fault type: %d", type);
        break;
    }
}
