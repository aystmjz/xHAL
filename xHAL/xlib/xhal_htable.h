#ifndef __XHAL_HTABLE_H
#define __XHAL_HTABLE_H

#include "../xcore/xhal_def.h"
#include "../xcore/xhal_std.h"

#define XHASH_SEEK_TIMES_MAX (32)

typedef struct xhal_htable_data
{
    uint32_t hash_time33;
    uint32_t hash_elf;
    uint32_t hash_bkdr;
    void *data;
} xhal_htable_data_t;

typedef struct hash_table
{
    uint32_t capacity;
    uint32_t prime_max;
    xhal_htable_data_t *table;
} xhal_htable_t;

xhal_htable_t *xhtable_new(uint32_t capacity);
void xhtable_destroy(xhal_htable_t *const self);
void xhtable_init(xhal_htable_t *const self, xhal_htable_data_t *table,
                  uint32_t capacity);
xhal_err_t xhtable_add(xhal_htable_t *const self, char *name, void *data);
xhal_err_t xhtable_remove(xhal_htable_t *const self, char *name);
void *xhtable_get(xhal_htable_t *const self, char *name);
uint8_t xhtable_existent(xhal_htable_t *const self, char *name);
int32_t xhtable_index(xhal_htable_t *const self, char *name);

#endif /* __XHAL_HTABLE_H */
