#ifndef __XHAL_MALLOC_H
#define __XHAL_MALLOC_H

#include "xhal_config.h"
#include "xhal_std.h"

#ifdef XHAL_OS_SUPPORTING
#include "../xos/xhal_os.h"

#define XMALLOC_ENTER_CRITICAL() osKernelLock()
#define XMALLOC_EXIT_CRITICAL()  osKernelUnlock()
#else
#define XMALLOC_ENTER_CRITICAL()
#define XMALLOC_EXIT_CRITICAL()
#endif

/* mem1内存参数设定.mem1是F103内部的SRAM. */
#ifndef XMALLOC_BLOCK_SIZE
#define XMALLOC_BLOCK_SIZE 32 /* 内存块大小为32字节 */
#endif

#ifndef XMALLOC_MAX_SIZE
#define XMALLOC_MAX_SIZE 15 * 1024 /* 最大管理内存 40K, F103内部SRAM总共512KB */
#endif

/* 内存表大小*/
#define XMALLOC_ALLOC_TABLE_SIZE (XMALLOC_MAX_SIZE / XMALLOC_BLOCK_SIZE)

/* 内存管理控制器 */
struct _m_mallco_dev
{
    uint8_t *const membase; /* 内存池 */
    uint16_t *const memmap; /* 内存管理状态表 */
    uint8_t memrdy;         /* 内存管理是否就绪 */
};

extern struct _m_mallco_dev mallco_dev;

uint32_t xmem_free_size(void);
uint16_t xmem_perused(void);

void xmemset(void *s, uint8_t c, uint32_t count);
void xmemcpy(void *des, const void *src, uint32_t n);

void xfree(void *ptr);
void *xmalloc(uint32_t size);
void *xcalloc(uint32_t n, uint32_t size);
void *xrealloc(void *ptr, uint32_t size);

#endif /* __XHAL_MALLOC_H */