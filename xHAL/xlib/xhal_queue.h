#ifndef __XHAL_QUEUE_H
#define __XHAL_QUEUE_H

#include "../xcore/xhal_std.h"

typedef struct xhal_queue
{
    uint8_t *buffer;
    uint16_t head;
    uint16_t tail;
    uint16_t capacity;
    uint16_t size_free;
} xhal_queue_t;

void xqueue_init(xhal_queue_t *const self, void *buffer, uint16_t capacity);
int32_t xqueue_push(xhal_queue_t *const self, void *buffer, uint16_t size);
uint16_t xqueue_pull(xhal_queue_t *const self, void *buffer, uint16_t size);
uint16_t xqueue_pop(xhal_queue_t *const self, uint16_t size);
void xqueue_clear(xhal_queue_t *const self);
uint16_t xqueue_pull_pop(xhal_queue_t *const self, void *buffer, uint16_t size);
uint16_t xqueue_free_size(xhal_queue_t *const self);
uint8_t xqueue_is_empty(xhal_queue_t *const self);
uint8_t xqueue_is_full(xhal_queue_t *const self);

#endif /* __XHAL_QUEUE_H */
