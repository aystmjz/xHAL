#include "xhal_queue.h"
#include "../xcore/xhal_def.h"

/**
 * @brief  Queue initialization.
 * @param  self          this pointer
 * @param  buffer      Queue's buffer memory.
 * @param  capacity    Queue's capacity.
 * @retval None
 */
void xqueue_init(xhal_queue_t *const self, void *buffer, uint16_t capacity)
{
    self->buffer    = (uint8_t *)buffer;
    self->capacity  = capacity;
    self->size_free = capacity;
    self->head      = 0;
    self->tail      = 0;
}

/**
 * @brief  Push data into queue.
 * @param  self      this pointer
 * @param  buffer  data buffer memory.
 * @param  size    data's size.
 * @retval if > 0, the data size; if < 0, error id.
 */
int32_t xqueue_push(xhal_queue_t *const self, void *buffer, uint16_t size)
{
    int32_t ret = (int32_t)size;

    if (size > self->size_free)
    {
        ret = XHAL_ERR_NOT_ENOUGH;
    }
    else
    {
        for (uint16_t i = 0; i < size; i++)
        {
            self->buffer[self->head] = ((uint8_t *)buffer)[i];
            self->head               = (self->head + 1) % self->capacity;
            self->size_free--;
        }
    }

    return ret;
}

/**
 * @brief  Pull data from queue, but the data is not poped.
 * @param  self      this pointer
 * @param  buffer  data buffer memory.
 * @param  size    data's size.
 * @retval Actual pulled data size.
 */
uint16_t xqueue_pull(xhal_queue_t *const self, void *buffer, uint16_t size)
{
    uint16_t size_data = self->capacity - self->size_free;
    uint16_t size_pull = size > size_data ? size_data : size;

    for (uint16_t i = 0; i < size_pull; i++)
    {
        ((uint8_t *)buffer)[i] =
            self->buffer[(self->tail + i) % self->capacity];
    }

    return size_pull;
}

/**
 * @brief  Pop data from queue.
 * @param  self      this pointer
 * @param  size    data's size.
 * @retval Actual poped data size.
 */
uint16_t xqueue_pop(xhal_queue_t *const self, uint16_t size)
{
    uint16_t size_data = self->capacity - self->size_free;
    uint16_t size_pop  = size > size_data ? size_data : size;

    self->tail = (self->tail + size_pop) % self->capacity;
    self->size_free += size_pop;

    return size_pop;
}

/**
 * @brief  Pull and pop data from queue, and the data is poped.
 * @param  self      this pointer
 * @param  buffer  data buffer memory.
 * @param  size    data's size.
 * @retval Actual pulled & poped data size.
 */
uint16_t xqueue_pull_pop(xhal_queue_t *const self, void *buffer, uint16_t size)
{
    uint16_t size_data = self->capacity - self->size_free;
    uint16_t size_pop  = size > size_data ? size_data : size;

    for (uint16_t i = 0; i < size_pop; i++)
    {
        ((uint8_t *)buffer)[i] =
            self->buffer[(self->tail + i) % self->capacity];
    }
    self->tail = (self->tail + size_pop) % self->capacity;
    self->size_free += size_pop;

    return size_pop;
}

/**
 * @brief  Clear all the data of the queue.
 * @param  self          this pointer
 * @retval Free size.
 */
void xqueue_clear(xhal_queue_t *const self)
{
    self->size_free = self->capacity;
    self->head      = 0;
    self->tail      = 0;
}

/**
 * @brief  Get the free size of the queue.
 * @param  self          this pointer
 * @retval Free size.
 */
uint16_t xqueue_free_size(xhal_queue_t *const self)
{
    return self->size_free;
}

/**
 * @brief  Check the queue is empty or not.
 * @param  self          this pointer
 * @retval Empty or not.
 */
uint8_t xqueue_is_empty(xhal_queue_t *const self)
{
    return self->size_free == self->capacity ? true : false;
}

/**
 * @brief  Check the queue is full or not.
 * @param  self          this pointer
 * @retval Full or not.
 */
uint8_t xqueue_is_full(xhal_queue_t *const self)
{
    return self->size_free == 0 ? true : false;
}
