#ifndef __XHAL_MALLOC_H
#define __XHAL_MALLOC_H

#include "xhal_config.h"
#include "xhal_std.h"

typedef struct xmem_pool
{
    uint8_t *const membase;
    uint16_t *const memmap;
    uint8_t memrdy;
} xmem_pool_t;

uint32_t xmem_free_size(void);
uint16_t xmem_perused(void);

void xmemset(void *s, uint8_t c, uint32_t count);
void xmemcpy(void *des, const void *src, uint32_t n);

void xfree(void *ptr);
void *xmalloc(uint32_t size);
void *xcalloc(uint32_t n, uint32_t size);
void *xrealloc(void *ptr, uint32_t size);

#endif /* __XHAL_MALLOC_H */