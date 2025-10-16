/*
 * Copyright (c) 2024 Tilen MAJERLE
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file is part of LwRB - Lightweight ring buffer library.
 *
 * Author:          Tilen MAJERLE <tilen@majerle.eu>
 * Version:         v3.2.0
 */
#ifndef __XRBUF_H
#define __XRBUF_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define XRBUF_DISABLE_ATOMIC

/**
 * \defgroup        LWRB Lightweight ring buffer manager
 * \brief           Lightweight ring buffer manager
 * \{
 */

#if !defined(XRBUF_DISABLE_ATOMIC) || __DOXYGEN__
#include <stdatomic.h>

/**
 * \brief           Atomic type for size variable.
 * Default value is set to be `unsigned 32-bits` type
 */
typedef atomic_ulong xrbuf_sz_atomic_t;
#else
typedef unsigned long xrbuf_sz_atomic_t;
#endif

/**
 * \brief           Event type for buffer operations
 */
typedef enum
{
    XRBUF_EVT_READ,  /*!< Read event */
    XRBUF_EVT_WRITE, /*!< Write event */
    XRBUF_EVT_RESET, /*!< Reset event */
} xrbuf_evt_type_t;

/**
 * \brief           Buffer structure forward declaration
 */
struct lwrb;

/**
 * \brief           Event callback function type
 * \param[in]       buff: Buffer handle for event
 * \param[in]       evt: Event type
 * \param[in]       bp: Number of bytes written or read (when used), depends on
 * event type
 */
typedef void (*xrbuf_evt_fn)(struct lwrb *buff, xrbuf_evt_type_t evt,
                             uint32_t bp);

/* List of flags */
#define XRBUF_FLAG_READ_ALL  ((uint16_t)0x0001)
#define XRBUF_FLAG_WRITE_ALL ((uint16_t)0x0001)

/**
 * \brief           Buffer structure
 */
typedef struct lwrb
{
    uint8_t *buff; /*!< Pointer to buffer data. Buffer is considered initialized
                      when `buff != NULL` and `size > 0` */
    uint32_t size; /*!< Size of buffer data. Size of actual buffer is `1` byte
                        less than value holds */
    xrbuf_sz_atomic_t r_ptr; /*!< Next read pointer.
                                Buffer is considered empty when `r == w` and
                                full when `w == r - 1` */
    xrbuf_sz_atomic_t w_ptr; /*!< Next write pointer.
                                Buffer is considered empty when `r == w` and
                                full when `w == r - 1` */
    xrbuf_evt_fn evt_fn;     /*!< Pointer to event callback function */
    void *arg;               /*!< Event custom user argument */
} xrbuf_t;

uint8_t xrbuf_init(xrbuf_t *buff, void *buffdata, uint32_t size);
uint8_t xrbuf_is_ready(xrbuf_t *buff);
void xrbuf_free(xrbuf_t *buff);
void xrbuf_reset(xrbuf_t *buff);
void xrbuf_set_evt_fn(xrbuf_t *buff, xrbuf_evt_fn fn);
void xrbuf_set_arg(xrbuf_t *buff, void *arg);
void *xrbuf_get_arg(xrbuf_t *buff);

/* Read/Write functions */

uint32_t xrbuf_write(xrbuf_t *buff, const void *data, uint32_t btw);
uint32_t xrbuf_read(xrbuf_t *buff, void *data, uint32_t btr);
uint32_t xrbuf_peek(const xrbuf_t *buff, uint32_t skip_count, void *data,
                    uint32_t btp);

/* Extended read/write functions */

uint8_t xrbuf_write_ex(xrbuf_t *buff, const void *data, uint32_t btw,
                       uint32_t *bwritten, uint16_t flags);
uint8_t xrbuf_read_ex(xrbuf_t *buff, void *data, uint32_t btr, uint32_t *bread,
                      uint16_t flags);

/* Buffer size information */

uint32_t xrbuf_get_free(const xrbuf_t *buff);
uint32_t xrbuf_get_full(const xrbuf_t *buff);

/* Read data block management */

void *xrbuf_get_linear_block_read_address(const xrbuf_t *buff);
uint32_t xrbuf_get_linear_block_read_length(const xrbuf_t *buff);
uint32_t xrbuf_skip(xrbuf_t *buff, uint32_t len);

/* Write data block management */

void *xrbuf_get_linear_block_write_address(const xrbuf_t *buff);
uint32_t xrbuf_get_linear_block_write_length(const xrbuf_t *buff);
uint32_t xrbuf_advance(xrbuf_t *buff, uint32_t len);

/* Search in buffer */

uint8_t xrbuf_find(const xrbuf_t *buff, const void *bts, uint32_t len,
                   uint32_t start_offset, uint32_t *found_idx);
uint32_t xrbuf_overwrite(xrbuf_t *buff, const void *data, uint32_t btw);
uint32_t xrbuf_move(xrbuf_t *dest, xrbuf_t *src);

#endif /* __XRBUF_H */
