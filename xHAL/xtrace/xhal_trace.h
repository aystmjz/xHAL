#ifndef __XHAL_TRACE_H
#define __XHAL_TRACE_H

#define FAULT_DIV0       1 // 除零错误
#define FAULT_UNALIGN    2 // 非对齐访问
#define FAULT_INV_ADDR   3 // 访问非法地址
#define FAULT_STACK_OVER 4 // 栈溢出
#define FAULT_INV_EXEC   5 // 非法执行（执行数据区）

extern void fault_trigger(uint8_t type);

#include "cm_backtrace/cm_backtrace.h"
#include "xhal_config.h"

#endif /* __XHAL_TRACE_H */