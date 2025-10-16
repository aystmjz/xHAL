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
#include "xhal_ringbuf.h"
#include "../xcore/xhal_malloc.h"

/* Memory set and copy functions */
#define BUF_MEMSET      xmemset
#define BUF_MEMCPY      xmemcpy

#define BUF_IS_VALID(b) ((b) != NULL && (b)->buff != NULL && (b)->size > 0)
#define BUF_MIN(x, y)   ((x) < (y) ? (x) : (y))
#define BUF_MAX(x, y)   ((x) > (y) ? (x) : (y))
#define BUF_SEND_EVT(b, type, bp)                                                                                      \
    do {                                                                                                               \
        if ((b)->evt_fn != NULL) {                                                                                     \
            (b)->evt_fn((void*)(b), (type), (bp));                                                                     \
        }                                                                                                              \
    } while (0)

/* Optional atomic opeartions */
#ifdef XRBUF_DISABLE_ATOMIC
#define XRBUF_INIT(var, val)        (var) = (val)
#define XRBUF_LOAD(var, type)       (var)
#define XRBUF_STORE(var, val, type) (var) = (val)
#else
#define XRBUF_INIT(var, val)        atomic_init(&(var), (val))
#define XRBUF_LOAD(var, type)       atomic_load_explicit(&(var), (type))
#define XRBUF_STORE(var, val, type) atomic_store_explicit(&(var), (val), (type))
#endif

/**
 * \brief           Initialize buffer handle to default values with size and buffer data array
 * \param[in]       buff: Ring buffer instance
 * \param[in]       buffdata: Pointer to memory to use as buffer data
 * \param[in]       size: Size of `buffdata` in units of bytes
 *                      Maximum number of bytes buffer can hold is `size - 1`
 * \return          `1` on success, `0` otherwise
 */
uint8_t
xrbuf_init(xrbuf_t* buff, void* buffdata, uint32_t size) {
    if (buff == NULL || buffdata == NULL || size == 0) {
        return 0;
    }

    buff->evt_fn = NULL;
    buff->size = size;
    buff->buff = buffdata;
    XRBUF_INIT(buff->w_ptr, 0);
    XRBUF_INIT(buff->r_ptr, 0);
    return 1;
}

/**
 * \brief           Check if buff is initialized and ready to use
 * \param[in]       buff: Ring buffer instance
 * \return          `1` if ready, `0` otherwise
 */
uint8_t
xrbuf_is_ready(xrbuf_t* buff) {
    return BUF_IS_VALID(buff);
}

/**
 * \brief           Free buffer memory
 * \note            Since implementation does not use dynamic allocation,
 *                  it just sets buffer handle to `NULL`
 * \param[in]       buff: Ring buffer instance
 */
void
xrbuf_free(xrbuf_t* buff) {
    if (BUF_IS_VALID(buff)) {
        buff->buff = NULL;
    }
}

/**
 * \brief           Set event function callback for different buffer operations
 * \param[in]       buff: Ring buffer instance
 * \param[in]       evt_fn: Callback function
 */
void
xrbuf_set_evt_fn(xrbuf_t* buff, xrbuf_evt_fn evt_fn) {
    if (BUF_IS_VALID(buff)) {
        buff->evt_fn = evt_fn;
    }
}

/**
 * \brief           Set custom buffer argument, that can be retrieved in the event function
 * \param[in]       buff: Ring buffer instance
 * \param[in]       arg: Custom user argument
 */
void
xrbuf_set_arg(xrbuf_t* buff, void* arg) {
    if (BUF_IS_VALID(buff)) {
        buff->arg = arg;
    }
}

/**
 * \brief           Get custom buffer argument, previously set with \ref xrbuf_set_arg
 * \param[in]       buff: Ring buffer instance
 * \return          User argument, previously set with \ref xrbuf_set_arg
 */
void*
xrbuf_get_arg(xrbuf_t* buff) {
    return buff != NULL ? buff->arg : NULL;
}

/**
 * \brief           Write data to buffer.
 *                  Copies data from `data` array to buffer and advances the write pointer for a maximum of `btw` number of bytes.
 * 
 *                  It copies less if there is less memory available in the buffer.
 *                  User must check the return value of the function and compare it to
 *                  the requested write length, to determine if everything has been written
 * 
 * \note            Use \ref xrbuf_write_ex for more advanced usage
 *
 * \param[in]       buff: Ring buffer instance
 * \param[in]       data: Pointer to data to write into buffer
 * \param[in]       btw: Number of bytes to write
 * \return          Number of bytes written to buffer.
 *                      When returned value is less than `btw`, there was no enough memory available
 *                      to copy full data array.
 */
uint32_t
xrbuf_write(xrbuf_t* buff, const void* data, uint32_t btw) {
    uint32_t written = 0;

    if (xrbuf_write_ex(buff, data, btw, &written, 0)) {
        return written;
    }
    return 0;
}

/**
 * \brief           Write extended functionality
 * 
 * \param           buff: Ring buffer instance
 * \param           data: Pointer to data to write into buffer
 * \param           btw: Number of bytes to write
 * \param           bwritten: Output pointer to write number of bytes written into the buffer
 * \param           flags: Optional flags.
 *                      \ref XRBUF_FLAG_WRITE_ALL: Request to write all data (up to btw).
 *                          Will early return if no memory available
 * \return          `1` if write operation OK, `0` otherwise
 */
uint8_t
xrbuf_write_ex(xrbuf_t* buff, const void* data, uint32_t btw, uint32_t* bwritten, uint16_t flags) {
    uint32_t tocopy = 0, free = 0, w_ptr = 0;
    const uint8_t* d_ptr = data;

    if (!BUF_IS_VALID(buff) || data == NULL || btw == 0) {
        return 0;
    }

    /* Calculate maximum number of bytes available to write */
    free = xrbuf_get_free(buff);
    /* If no memory, or if user wants to write ALL data but no enough space, exit early */
    if (free == 0 || (free < btw && (flags & XRBUF_FLAG_WRITE_ALL))) {
        return 0;
    }
    btw = BUF_MIN(free, btw);
    w_ptr = XRBUF_LOAD(buff->w_ptr, memory_order_acquire);

    /* Step 1: Write data to linear part of buffer */
    tocopy = BUF_MIN(buff->size - w_ptr, btw);
    BUF_MEMCPY(&buff->buff[w_ptr], d_ptr, tocopy);
    d_ptr += tocopy;
    w_ptr += tocopy;
    btw -= tocopy;

    /* Step 2: Write data to beginning of buffer (overflow part) */
    if (btw > 0) {
        BUF_MEMCPY(buff->buff, d_ptr, btw);
        w_ptr = btw;
    }

    /* Step 3: Check end of buffer */
    if (w_ptr >= buff->size) {
        w_ptr = 0;
    }

    /*
     * Write final value to the actual running variable.
     * This is to ensure no read operation can access intermediate data
     */
    XRBUF_STORE(buff->w_ptr, w_ptr, memory_order_release);

    BUF_SEND_EVT(buff, XRBUF_EVT_WRITE, tocopy + btw);
    if (bwritten != NULL) {
        *bwritten = tocopy + btw;
    }
    return 1;
}

/**
 * \brief           Read data from buffer.
 *                  Copies data from `data` array to buffer and advances the read pointer for a maximum of `btr` number of bytes.
 * 
 *                  It copies less if there is less data available in the buffer.
 * 
 * \note            Use \ref xrbuf_read_ex for more advanced usage
 *
 * \param[in]       buff: Ring buffer instance
 * \param[out]      data: Pointer to output memory to copy buffer data to
 * \param[in]       btr: Number of bytes to read
 * \return          Number of bytes read and copied to data array
 */
uint32_t
xrbuf_read(xrbuf_t* buff, void* data, uint32_t btr) {
    uint32_t read = 0;

    if (xrbuf_read_ex(buff, data, btr, &read, 0)) {
        return read;
    }
    return 0;
}

/**
 * \brief           Read extended functionality
 * 
 * \param           buff: Ring buffer instance
 * \param           data: Pointer to memory to write read data from buffer 
 * \param           btr: Number of bytes to read
 * \param           bread: Output pointer to write number of bytes read from buffer and written to the 
 *                      output `data` variable
 * \param           flags: Optional flags
 *                      \ref XRBUF_FLAG_READ_ALL: Request to read all data (up to btr).
 *                          Will early return if no enough bytes in the buffer
 * \return          `1` if read operation OK, `0` otherwise
 */
uint8_t
xrbuf_read_ex(xrbuf_t* buff, void* data, uint32_t btr, uint32_t* bread, uint16_t flags) {
    uint32_t tocopy = 0, full = 0, r_ptr = 0;
    uint8_t* d_ptr = data;

    if (!BUF_IS_VALID(buff) || data == NULL || btr == 0) {
        return 0;
    }

    /* Calculate maximum number of bytes available to read */
    full = xrbuf_get_full(buff);
    if (full == 0 || (full < btr && (flags & XRBUF_FLAG_READ_ALL))) {
        return 0;
    }
    btr = BUF_MIN(full, btr);
    r_ptr = XRBUF_LOAD(buff->r_ptr, memory_order_acquire);

    /* Step 1: Read data from linear part of buffer */
    tocopy = BUF_MIN(buff->size - r_ptr, btr);
    BUF_MEMCPY(d_ptr, &buff->buff[r_ptr], tocopy);
    d_ptr += tocopy;
    r_ptr += tocopy;
    btr -= tocopy;

    /* Step 2: Read data from beginning of buffer (overflow part) */
    if (btr > 0) {
        BUF_MEMCPY(d_ptr, buff->buff, btr);
        r_ptr = btr;
    }

    /* Step 3: Check end of buffer */
    if (r_ptr >= buff->size) {
        r_ptr = 0;
    }

    /*
     * Write final value to the actual running variable.
     * This is to ensure no write operation can access intermediate data
     */
    XRBUF_STORE(buff->r_ptr, r_ptr, memory_order_release);

    BUF_SEND_EVT(buff, XRBUF_EVT_READ, tocopy + btr);
    if (bread != NULL) {
        *bread = tocopy + btr;
    }
    return 1;
}

/**
 * \brief           Read from buffer without changing read pointer (peek only)
 * \param[in]       buff: Ring buffer instance
 * \param[in]       skip_count: Number of bytes to skip before reading data
 * \param[out]      data: Pointer to output memory to copy buffer data to
 * \param[in]       btp: Number of bytes to peek
 * \return          Number of bytes peeked and written to output array
 */
uint32_t
xrbuf_peek(const xrbuf_t* buff, uint32_t skip_count, void* data, uint32_t btp) {
    uint32_t full = 0, tocopy = 0, r_ptr = 0;
    uint8_t* d_ptr = data;

    if (!BUF_IS_VALID(buff) || data == NULL || btp == 0) {
        return 0;
    }

    /*
     * Calculate maximum number of bytes available to read
     * and check if we can even fit to it
     */
    full = xrbuf_get_full(buff);
    if (skip_count >= full) {
        return 0;
    }
    r_ptr = XRBUF_LOAD(buff->r_ptr, memory_order_relaxed);
    r_ptr += skip_count;
    full -= skip_count;
    if (r_ptr >= buff->size) {
        r_ptr -= buff->size;
    }

    /* Check maximum number of bytes available to read after skip */
    btp = BUF_MIN(full, btp);
    if (btp == 0) {
        return 0;
    }

    /* Step 1: Read data from linear part of buffer */
    tocopy = BUF_MIN(buff->size - r_ptr, btp);
    BUF_MEMCPY(d_ptr, &buff->buff[r_ptr], tocopy);
    d_ptr += tocopy;
    btp -= tocopy;

    /* Step 2: Read data from beginning of buffer (overflow part) */
    if (btp > 0) {
        BUF_MEMCPY(d_ptr, buff->buff, btp);
    }
    return tocopy + btp;
}

/**
 * \brief           Get available size in buffer for write operation
 * \param[in]       buff: Ring buffer instance
 * \return          Number of free bytes in memory
 */
uint32_t
xrbuf_get_free(const xrbuf_t* buff) {
    uint32_t size = 0, w_ptr = 0, r_ptr = 0;

    if (!BUF_IS_VALID(buff)) {
        return 0;
    }

    /*
     * Copy buffer pointers to local variables with atomic access.
     *
     * To ensure thread safety (only when in single-entry, single-exit FIFO mode use case),
     * it is important to write buffer r and w values to local w and r variables.
     *
     * Local variables will ensure below if statements will always use the same value,
     * even if buff->w or buff->r get changed during interrupt processing.
     *
     * They may change during load operation, important is that
     * they do not change during if-else operations following these assignments.
     *
     * xrbuf_get_free is only called for write purpose, and when in FIFO mode, then:
     * - buff->w pointer will not change by another process/interrupt because we are in write mode just now
     * - buff->r pointer may change by another process. If it gets changed after buff->r has been loaded to local variable,
     *    buffer will see "free size" less than it actually is. This is not a problem, application can
     *    always try again to write more data to remaining free memory that was read just during copy operation
     */
    w_ptr = XRBUF_LOAD(buff->w_ptr, memory_order_relaxed);
    r_ptr = XRBUF_LOAD(buff->r_ptr, memory_order_relaxed);

    if (w_ptr >= r_ptr) {
        size = buff->size - (w_ptr - r_ptr);
    } else {
        size = r_ptr - w_ptr;
    }

    /* Buffer free size is always 1 less than actual size */
    return size - 1;
}

/**
 * \brief           Get number of bytes currently available in buffer
 * \param[in]       buff: Ring buffer instance
 * \return          Number of bytes ready to be read
 */
uint32_t
xrbuf_get_full(const xrbuf_t* buff) {
    uint32_t size = 0, w_ptr = 0, r_ptr = 0;

    if (!BUF_IS_VALID(buff)) {
        return 0;
    }

    /*
     * Copy buffer pointers to local variables.
     *
     * To ensure thread safety (only when in single-entry, single-exit FIFO mode use case),
     * it is important to write buffer r and w values to local w and r variables.
     *
     * Local variables will ensure below if statements will always use the same value,
     * even if buff->w or buff->r get changed during interrupt processing.
     *
     * They may change during load operation, important is that
     * they do not change during if-else operations following these assignments.
     *
     * xrbuf_get_full is only called for read purpose, and when in FIFO mode, then:
     * - buff->r pointer will not change by another process/interrupt because we are in read mode just now
     * - buff->w pointer may change by another process. If it gets changed after buff->w has been loaded to local variable,
     *    buffer will see "full size" less than it really is. This is not a problem, application can
     *    always try again to read more data from remaining full memory that was written just during copy operation
     */
    w_ptr = XRBUF_LOAD(buff->w_ptr, memory_order_relaxed);
    r_ptr = XRBUF_LOAD(buff->r_ptr, memory_order_relaxed);

    if (w_ptr >= r_ptr) {
        size = w_ptr - r_ptr;
    } else {
        size = buff->size - (r_ptr - w_ptr);
    }
    return size;
}

/**
 * \brief           Resets buffer to default values. Buffer size is not modified
 * \note            This function is not thread safe.
 *                      When used, application must ensure there is no active read/write operation
 * \param[in]       buff: Ring buffer instance
 */
void
xrbuf_reset(xrbuf_t* buff) {
    if (BUF_IS_VALID(buff)) {
        XRBUF_STORE(buff->w_ptr, 0, memory_order_release);
        XRBUF_STORE(buff->r_ptr, 0, memory_order_release);
        BUF_SEND_EVT(buff, XRBUF_EVT_RESET, 0);
    }
}

/**
 * \brief           Get linear address for buffer for fast read
 * \param[in]       buff: Ring buffer instance
 * \return          Linear buffer start address
 */
void*
xrbuf_get_linear_block_read_address(const xrbuf_t* buff) {
    uint32_t ptr = 0;

    if (!BUF_IS_VALID(buff)) {
        return NULL;
    }
    ptr = XRBUF_LOAD(buff->r_ptr, memory_order_relaxed);
    return &buff->buff[ptr];
}

/**
 * \brief           Get length of linear block address before it overflows for read operation
 * \param[in]       buff: Ring buffer instance
 * \return          Linear buffer size in units of bytes for read operation
 */
uint32_t
xrbuf_get_linear_block_read_length(const xrbuf_t* buff) {
    uint32_t len = 0, w_ptr = 0, r_ptr = 0;

    if (!BUF_IS_VALID(buff)) {
        return 0;
    }

    /*
     * Use temporary values in case they are changed during operations.
     * See xrbuf_buff_free or xrbuf_buff_full functions for more information why this is OK.
     */
    w_ptr = XRBUF_LOAD(buff->w_ptr, memory_order_relaxed);
    r_ptr = XRBUF_LOAD(buff->r_ptr, memory_order_relaxed);

    if (w_ptr > r_ptr) {
        len = w_ptr - r_ptr;
    } else if (r_ptr > w_ptr) {
        len = buff->size - r_ptr;
    } else {
        len = 0;
    }
    return len;
}

/**
 * \brief           Skip (ignore; advance read pointer) buffer data
 * Marks data as read in the buffer and increases free memory for up to `len` bytes
 *
 * \note            Useful at the end of streaming transfer such as DMA
 * \param[in]       buff: Ring buffer instance
 * \param[in]       len: Number of bytes to skip and mark as read
 * \return          Number of bytes skipped
 */
uint32_t
xrbuf_skip(xrbuf_t* buff, uint32_t len) {
    uint32_t full = 0, r_ptr = 0;

    if (!BUF_IS_VALID(buff) || len == 0) {
        return 0;
    }

    full = xrbuf_get_full(buff);
    len = BUF_MIN(len, full);
    r_ptr = XRBUF_LOAD(buff->r_ptr, memory_order_acquire);
    r_ptr += len;
    if (r_ptr >= buff->size) {
        r_ptr -= buff->size;
    }
    XRBUF_STORE(buff->r_ptr, r_ptr, memory_order_release);
    BUF_SEND_EVT(buff, XRBUF_EVT_READ, len);
    return len;
}

/**
 * \brief           Get linear address for buffer for fast write
 * \param[in]       buff: Ring buffer instance
 * \return          Linear buffer start address
 */
void*
xrbuf_get_linear_block_write_address(const xrbuf_t* buff) {
    uint32_t ptr = 0;

    if (!BUF_IS_VALID(buff)) {
        return NULL;
    }
    ptr = XRBUF_LOAD(buff->w_ptr, memory_order_relaxed);
    return &buff->buff[ptr];
}

/**
 * \brief           Get length of linear block address before it overflows for write operation
 * \param[in]       buff: Ring buffer instance
 * \return          Linear buffer size in units of bytes for write operation
 */
uint32_t
xrbuf_get_linear_block_write_length(const xrbuf_t* buff) {
    uint32_t len = 0, w_ptr = 0, r_ptr = 0;

    if (!BUF_IS_VALID(buff)) {
        return 0;
    }

    /*
     * Use temporary values in case they are changed during operations.
     * See xrbuf_buff_free or xrbuf_buff_full functions for more information why this is OK.
     */
    w_ptr = XRBUF_LOAD(buff->w_ptr, memory_order_relaxed);
    r_ptr = XRBUF_LOAD(buff->r_ptr, memory_order_relaxed);

    if (w_ptr >= r_ptr) {
        len = buff->size - w_ptr;
        /*
         * When read pointer is 0,
         * maximal length is one less as if too many bytes
         * are written, buffer would be considered empty again (r == w)
         */
        if (r_ptr == 0) {
            /*
             * Cannot overflow:
             * - If r is not 0, statement does not get called
             * - buff->size cannot be 0 and if r is 0, len is greater 0
             */
            --len;
        }
    } else {
        len = r_ptr - w_ptr - 1;
    }
    return len;
}

/**
 * \brief           Advance write pointer in the buffer.
 * Similar to skip function but modifies write pointer instead of read
 *
 * \note            Useful when hardware is writing to buffer and application needs to increase number
 *                      of bytes written to buffer by hardware
 * \param[in]       buff: Ring buffer instance
 * \param[in]       len: Number of bytes to advance
 * \return          Number of bytes advanced for write operation
 */
uint32_t
xrbuf_advance(xrbuf_t* buff, uint32_t len) {
    uint32_t free = 0, w_ptr = 0;

    if (!BUF_IS_VALID(buff) || len == 0) {
        return 0;
    }

    /* Use local variables before writing back to main structure */
    free = xrbuf_get_free(buff);
    len = BUF_MIN(len, free);
    w_ptr = XRBUF_LOAD(buff->w_ptr, memory_order_acquire);
    w_ptr += len;
    if (w_ptr >= buff->size) {
        w_ptr -= buff->size;
    }
    XRBUF_STORE(buff->w_ptr, w_ptr, memory_order_release);
    BUF_SEND_EVT(buff, XRBUF_EVT_WRITE, len);
    return len;
}

/**
 * \brief           Searches for a *needle* in an array, starting from given offset.
 * 
 * \note            This function is not thread-safe. 
 * 
 * \param           buff: Ring buffer to search for needle in
 * \param           bts: Constant byte array sequence to search for in a buffer
 * \param           len: Length of the \arg bts array 
 * \param           start_offset: Start offset in the buffer
 * \param           found_idx: Pointer to variable to write index in array where bts has been found
 *                      Must not be set to `NULL`
 * \return          `1` if \arg bts found, `0` otherwise
 */
uint8_t
xrbuf_find(const xrbuf_t* buff, const void* bts, uint32_t len, uint32_t start_offset, uint32_t* found_idx) {
    uint32_t full = 0, r_ptr = 0, buff_r_ptr = 0, max_x = 0;
    uint8_t found = 0;
    const uint8_t* needle = bts;

    if (!BUF_IS_VALID(buff) || needle == NULL || len == 0 || found_idx == NULL) {
        return 0;
    }
    *found_idx = 0;

    full = xrbuf_get_full(buff);
    /* Verify initial conditions */
    if (full < (len + start_offset)) {
        return 0;
    }

    /* Get actual buffer read pointer for this search */
    buff_r_ptr = XRBUF_LOAD(buff->r_ptr, memory_order_relaxed);

    /* Max number of for loops is buff_full - input_len - start_offset of buffer length */
    max_x = full - len;
    for (uint32_t skip_x = start_offset; !found && skip_x <= max_x; ++skip_x) {
        found = 1; /* Found by default */

        /* Prepare the starting point for reading */
        r_ptr = buff_r_ptr + skip_x;
        if (r_ptr >= buff->size) {
            r_ptr -= buff->size;
        }

        /* Search in the buffer */
        for (uint32_t idx = 0; idx < len; ++idx) {
            if (buff->buff[r_ptr] != needle[idx]) {
                found = 0;
                break;
            }
            if (++r_ptr >= buff->size) {
                r_ptr = 0;
            }
        }
        if (found) {
            *found_idx = skip_x;
        }
    }
    return found;
}

#define BUF_IS_VALID(b) ((b) != NULL && (b)->buff != NULL && (b)->size > 0)
#define BUF_MIN(x, y)   ((x) < (y) ? (x) : (y))

/**
 * \brief           Writes data to buffer with overwrite function, if no enough space to hold
 *                  complete input data object.
 * \note            Similar to \ref xrbuf_write but overwrites
 * \param[in]       buff: Buffer handle
 * \param[in]       data: Data to write to ring buffer
 * \param[in]       btw: Bytes To Write, length
 * \return          Number of bytes written to buffer, will always return btw
 * \note            Functionality is primary two parts, always writes some linear region, then
 *                      writes the wrap region if there is more data to write. The r indicator is advanced if w overtakes
 *                      it. This operation is a read op as well as a write op. For thread-safety mutexes may be desired,
 *                      see documentation.
 */
uint32_t
xrbuf_overwrite(xrbuf_t* buff, const void* data, uint32_t btw) {
    uint32_t orig_btw = btw, max_cap;
    const uint8_t* d = data;

    if (!BUF_IS_VALID(buff) || data == NULL || btw == 0) {
        return 0;
    }

    /* Process complete input array */
    max_cap = buff->size - 1; /* Maximum capacity buffer can hold */
    if (btw > max_cap) {
        /*
         * When data to write is larger than max buffer capacity,
         * we can reset the buffer and simply write last part of 
         * the input buffer.
         * 
         * This is done here, by calculating remaining
         * length and then advancing to the end of input buffer
         */
        d += btw - max_cap; /* Advance data */
        btw = max_cap;      /* Limit data to write */
        xrbuf_reset(buff);   /* Reset buffer */
    } else {
        /* 
         * Bytes to write is less than capacity
         * We have to perform max one skip operation,
         * but only if free memory is less than
         * btw, otherwise we skip the operation
         * and only write the data.
         */
        uint32_t f = xrbuf_get_free(buff);
        if (f < btw) {
            xrbuf_skip(buff, btw - f);
        }
    }
    xrbuf_write(buff, d, btw);
    return orig_btw;
}

/**
 * \brief           Move one ring buffer to another, up to the amount of data in the source, or amount 
 *                      of data free in the destination.
 * \param[in]       dest: Buffer handle that the copied data will be written to
 * \param[in]       src:  Buffer handle that the copied data will come from.
 *                      Source buffer will be effectively read upon operation.
 * \return          Number of bytes written to destination buffer
 * \note            This operation is a read op to the source, on success it will update the r index. 
 *                  As well as a write op to the destination, and may update the w index.
 *                  For thread-safety mutexes may be desired, see documentation.
 */
uint32_t
xrbuf_move(xrbuf_t* dest, xrbuf_t* src) {
    uint32_t len_to_copy, len_to_copy_orig, src_full, dest_free;

    if (!BUF_IS_VALID(dest) || !BUF_IS_VALID(src)) {
        return 0;
    }
    src_full = xrbuf_get_full(src);
    dest_free = xrbuf_get_free(dest);
    len_to_copy = BUF_MIN(src_full, dest_free);
    len_to_copy_orig = len_to_copy;

    /* Calculations for available length to copy is done above.
        We safely assume operations inside loop will properly complete. */
    while (len_to_copy > 0) {
        uint32_t max_seq_read, max_seq_write, op_len;
        const uint8_t* d_src;
        uint8_t* d_dst;

        /* Calculate data */
        max_seq_read = xrbuf_get_linear_block_read_length(src);
        max_seq_write = xrbuf_get_linear_block_write_length(dest);
        op_len = BUF_MIN(max_seq_read, max_seq_write);
        op_len = BUF_MIN(len_to_copy, op_len);

        /* Get addresses */
        d_src = xrbuf_get_linear_block_read_address(src);
        d_dst = xrbuf_get_linear_block_write_address(dest);

        /* Byte by byte copy */
        for (uint32_t i = 0; i < op_len; ++i) {
            *d_dst++ = *d_src++;
        }

        xrbuf_advance(dest, op_len);
        xrbuf_skip(src, op_len);
        len_to_copy -= op_len;
        if (op_len == 0) {
            /* Hard error... */
            return 0;
        }
    }
    return len_to_copy_orig;
}
