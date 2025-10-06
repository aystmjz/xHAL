#include "xhal_malloc.h"
#include "xhal_assert.h"
#include "xhal_def.h"
#include "xhal_export.h"
#include "xhal_log.h"

XLOG_TAG("xMalloc");

/* 内部SRAM内存池 */
static XHAL_USED XHAL_ALIGN(64) uint8_t mem1base[XMALLOC_MAX_SIZE];
/* 内部SRAM内存池MAP */
static uint16_t mem1mapbase[XMALLOC_ALLOC_TABLE_SIZE];

/* 内存管理控制器 */
struct _m_mallco_dev mallco_dev = {
    mem1base,    /* 内存池 */
    mem1mapbase, /* 内存管理状态表 */
    0,           /* 内存管理未就绪 */
};

/**
 * @brief  复制内存
 * @param  *des : 目的地址
 * @param  *src : 源地址
 * @param  n    : 需要复制的内存长度(字节为单位)
 * @retval 无
 */
void xmemcpy(void *des, void *src, uint32_t n)
{
    xassert_not_null(des);
    xassert_not_null(src);

    uint8_t *xdes = des;
    uint8_t *xsrc = src;
    while (n--)
        *xdes++ = *xsrc++;
}
/**
 * @brief  设置内存值
 * @param  *s    : 内存首地址
 * @param  c: 要设置的值
 * @param  count : 需要设置的内存大小(字节为单位)
 * @retval 无
 */
void xmemset(void *s, uint8_t c, uint32_t count)
{
    xassert_not_null(s);

    uint8_t *xs = s;
    while (count--)
        *xs++ = c;
}

uint32_t xmem_free_size(void)
{
    uint32_t free_blocks = 0;

    XMALLOC_ENTER_CRITICAL(); /* 进入临界区 */
    for (uint32_t i = 0; i < XMALLOC_ALLOC_TABLE_SIZE; i++)
    {
        if (mallco_dev.memmap[i] == 0)
            free_blocks++;
    }
    XMALLOC_EXIT_CRITICAL(); /* 离开临界区 */

    return (free_blocks * XMALLOC_BLOCK_SIZE);
}

/**
 * @brief  获取内存使用率
 * @retval 使用率(扩大了10倍,0~1000,代表0.0%~100.0%)
 */
uint16_t xmem_perused(void)
{
    uint32_t perused = 0, used = 0;

    used    = XMALLOC_MAX_SIZE - xmem_free_size();
    perused = (used * 1000) / XMALLOC_MAX_SIZE;

    return perused; // 放大10倍，0~1000
}

/**
 * @brief  内存分配(内部调用)
 * @param  memx : 所属内存块
 * @param  size : 要分配的内存大小(字节)
 * @retval 内存偏移地址
 *   @arg  0 ~ 0XFFFFFFFE : 有效的内存偏移地址
 *   @arg  0XFFFFFFFF: 无效的内存偏移地址
 */
static uint32_t my_mem_malloc(uint32_t size)
{
    signed long offset = 0;
    uint32_t nmemb;     /* 需要的内存块数 */
    uint32_t cmemb = 0; /* 连续空内存块数 */
    uint32_t i;

    if (!mallco_dev.memrdy) /* 未初始化,先执行初始化 */
    {
        uint8_t mttsize = sizeof(uint16_t); /* 获取memmap数组的类型长度*/
        xmemset(mallco_dev.memmap, 0,
                XMALLOC_ALLOC_TABLE_SIZE * mttsize); /* 内存状态表数据清零 */
        mallco_dev.memrdy = 1; /* 内存管理初始化OK */
    }

    if (size == 0)
    {
        return 0xFFFFFFFF; /* 不需要分配 */
    }

    nmemb = size / XMALLOC_BLOCK_SIZE; /* 获取需要分配的连续内存块数 */
    if (size % XMALLOC_BLOCK_SIZE)
        nmemb++;

    for (offset = XMALLOC_ALLOC_TABLE_SIZE - 1; offset >= 0;
         offset--) /* 搜索整个内存控制区 */
    {
        if (!mallco_dev.memmap[offset])
            cmemb++; /* 连续空内存块数增加 */
        else
            cmemb = 0; /* 连续内存块清零 */

        if (cmemb == nmemb) /* 标注内存块非空 */
        {
            for (i = 0; i < nmemb; i++)
                mallco_dev.memmap[offset + i] = nmemb;
            return (offset * XMALLOC_BLOCK_SIZE); /* 返回偏移地址 */
        }
    }

    return 0xFFFFFFFF; /* 未找到符合分配条件的内存块 */
}

/**
 * @brief  释放内存(内部调用)
 * @param  memx   : 所属内存块
 * @param  offset : 内存地址偏移
 * @retval 释放结果
 *   @arg  0, 释放成功;
 *   @arg  1, 释放失败;
 *   @arg  2, 超区域了(失败);
 */
static uint8_t my_mem_free(uint32_t offset)
{
    if (!mallco_dev.memrdy) /* 未初始化 */
    {
        return 1;
    } /* 未初始化 */

    if (offset < XMALLOC_MAX_SIZE) /* 偏移在内存池内. */
    {
        int index = offset / XMALLOC_BLOCK_SIZE; /* 偏移所在内存块号码 */
        int nmemb = mallco_dev.memmap[index];    /* 内存块数量 */

        for (int i = 0; i < nmemb; i++) /* 内存块清零 */
            mallco_dev.memmap[index + i] = 0;

        return 0;
    }
    return 2; /* 偏移超区了. */
}

/**
 * @brief  释放内存(外部调用)
 * @param  memx : 所属内存块
 * @param  ptr  : 内存首地址
 * @retval 无
 */
void xfree(void *ptr)
{
    if (ptr == NULL)
    {
#ifdef XDEBUG
        XLOG_WARN("xfree NULL pointer");
#endif
        return; /* 地址为0. */
    }

    uint32_t offset = (uint32_t)ptr - (uint32_t)mallco_dev.membase;

    XMALLOC_ENTER_CRITICAL(); /* 进入临界区 */
    my_mem_free(offset);      /* 释放内存 */
    XMALLOC_EXIT_CRITICAL();  /* 离开临界区 */
}
/**
 * @brief  分配内存(外部调用)
 * @param  memx : 所属内存块
 * @param  size : 要分配的内存大小(字节)
 * @retval 分配到的内存首地址.
 */
void *xmalloc(uint32_t size)
{
    void *ptr;

    if (size == 0)
    {
#ifdef XDEBUG
        XLOG_WARN("xmalloc 0 size");
#endif
        return NULL; /* 不需要分配 */
    }

    XMALLOC_ENTER_CRITICAL(); /* 进入临界区 */
    uint32_t offset = my_mem_malloc(size);
    XMALLOC_EXIT_CRITICAL(); /* 离开临界区 */

    if (offset == 0xFFFFFFFF)
    {
#ifdef XDEBUG
        XLOG_ERROR("No memory");
#endif
        ptr = NULL; /* 申请出错 */
    }
    else
    {
        /* 申请没问题, 返回首地址 */
        ptr = (void *)((uint32_t)mallco_dev.membase + offset);

#ifdef XDEBUG
        uint16_t perused = xmem_perused();
        if (perused > 990)
        {
            XLOG_ERROR("Memory almost full: %d.%d%%", perused / 10,
                       perused % 10);
        }
        else if (perused > 900)
        {
            XLOG_WARN("High memory usage: %d.%d%%", perused / 10, perused % 10);
        }
#endif
    }

    return ptr;
}

/**
 * @brief  分配并清零内存
 * @param  n    : 元素个数
 * @param  size : 单个元素大小(字节)
 * @retval 分配到的内存首地址，失败返回 NULL
 */
void *xcalloc(uint32_t n, uint32_t size)
{
    uint32_t total_size = n * size;
    void *ptr           = xmalloc(total_size);
    if (ptr != NULL)
    {
        xmemset(ptr, 0, total_size);
    }
    return ptr;
}

/**
 * @brief  重新分配内存(外部调用)
 * @param  memx : 所属内存块
 * @param  *ptr : 旧内存首地址
 * @param  size : 要分配的内存大小(字节)
 * @retval 新分配到的内存首地址.
 */
void *xrealloc(void *ptr, uint32_t size)
{
    xassert_not_null(ptr);

    uint32_t offset = my_mem_malloc(size);
    /* 申请出错 */
    if (offset == 0xFFFFFFFF)
        return NULL; /* 返回空(0) */

    void *new_ptr = (void *)((uint32_t)mallco_dev.membase + offset);

    xmemcpy(new_ptr, ptr, size); /* 拷贝旧内存内容到新内存 */
    xfree(ptr);                  /* 释放旧内存 */

    return new_ptr; /* 返回新内存首地址 */
}
