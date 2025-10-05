#include "xhal_htable.h"
#include "../xcore/xhal_assert.h"
#include "../xcore/xhal_log.h"
#include "../xcore/xhal_malloc.h"

XLOG_TAG("xHashTable");

static uint32_t _get_prime_max(uint32_t capacity);
static uint32_t _hash_time33(char *str);
static uint32_t _hash_bkdr(char *str);
static uint32_t _hash_elf(char *str);

/**
 * @brief  Newly create one hash table in the given capacity.
 * @param  size    The given capacity size.
 * @retval The hash table handle.
 */
xhal_htable_t *xhtable_new(uint32_t capacity)
{
    xhal_htable_t *self       = NULL;
    xhal_htable_data_t *table = NULL;

    self = xmalloc(sizeof(xhal_htable_t));
    xassert(self != NULL);

    table = xmalloc(sizeof(xhal_htable_data_t) * capacity);
    xassert(table != NULL);

    xhtable_init(self, table, capacity);

    return self;
}

/**
 * @brief  Destroy the hash table which is generate by the funciton
 * xhtable_new. All the related resources are freed in this function.
 * @param  self  The hash table handle.
 * @retval None.
 */
void xhtable_destroy(xhal_htable_t *const self)
{
    xassert_not_null(self);

    xfree(self->table);
    self->table = NULL;
    xfree(self);
}

/**
 * @brief  Initialize one hash table in the static mode.
 * @param  self          The hash table handle.
 * @param  data        The hash table data.
 * @param  capacity    The hash table capacity.
 * @retval None.
 */
void xhtable_init(xhal_htable_t *const self, xhal_htable_data_t *data,
                  uint32_t capacity)
{
    xassert_not_null(self);
    xassert_not_null(data);

    self->table    = data;
    self->capacity = capacity;

    for (uint32_t i = 0; i < capacity; i++)
    {
        self->table[i].hash_time33 = UINT32_MAX;
        self->table[i].hash_elf    = UINT32_MAX;
        self->table[i].hash_bkdr   = UINT32_MAX;
        self->table[i].data        = NULL;
    }

    self->prime_max = _get_prime_max(capacity);
}

/**
 * @brief  Add one data block into hash table by the given name .
 * @param  self          The hash table handle.
 * @param  name        The given key name.
 * @param  data        The data block.
 * @retval See xhal_err_t.
 */
xhal_err_t xhtable_add(xhal_htable_t *const self, char *name, void *data)
{
    xassert_not_null(self);
    xassert_not_null(name);
    xassert_not_null(data);

    uint32_t hash_time33 = _hash_time33(name);
    uint32_t hash_elf    = _hash_elf(name);
    uint32_t hash_bkdr   = _hash_bkdr(name);

    uint32_t index_start = hash_time33 % self->prime_max;
    uint32_t index;
    uint32_t times_count = 0;
    for (uint32_t i = 0; i < self->capacity; i++)
    {
        index = (index_start + i) % self->capacity;
        if (self->table[index].data == NULL)
        {
            self->table[index].data        = data;
            self->table[index].hash_time33 = hash_time33;
            self->table[index].hash_elf    = hash_elf;
            self->table[index].hash_bkdr   = hash_bkdr;
            return XHAL_OK;
        }
        else
        {
            times_count++;
            if (times_count > XHASH_SEEK_TIMES_MAX)
            {
                break;
            }
        }
    }
#ifdef XDEBUG
    XLOG_ERROR("XHAL_ERR_FULL");
#endif
    return XHAL_ERR_FULL;
}

/**
 * @brief  Remove one data block from hash table by the given name .
 * @param  self          The hash table handle.
 * @param  name        The given key name.
 * @retval See xhal_err_t.
 */
xhal_err_t xhtable_remove(xhal_htable_t *const self, char *name)
{
    xassert_not_null(self);
    xassert_not_null(name);

    int32_t ret = xhtable_index(self, name);

    if (ret != (int32_t)XHAL_ERROR)
    {
        self->table[ret].data = NULL;
        return XHAL_OK;
    }
#ifdef XDEBUG
    XLOG_ERROR("XHAL_ERROR");
#endif
    return XHAL_ERROR;
}

/**
 * @brief  Get the data block from hash table by the given name .
 * @param  self          The hash table handle.
 * @param  name        The given key name.
 * @retval The data block pointer.
 */
void *xhtable_get(xhal_htable_t *const self, char *name)
{
    xassert_not_null(self);
    xassert_not_null(name);

    int32_t ret = xhtable_index(self, name);

    if (ret != (int32_t)XHAL_ERROR)
    {
        return self->table[ret].data;
    }
#ifdef XDEBUG
    XLOG_ERROR("XHAL_ERROR");
#endif
    return NULL;
}

/**
 * @brief  Check the data block is existent or not in hash table by the given
 *         name .
 * @param  self          The hash table handle.
 * @param  name        The given key name.
 * @retval True or false.
 */
uint8_t xhtable_existent(xhal_htable_t *const self, char *name)
{
    xassert_not_null(self);
    xassert_not_null(name);

    return (xhtable_index(self, name) == (int32_t)XHAL_ERROR) ? false : true;
}

/**
 * @brief  Get the data block's index from hash table by the given name.
 * @param  self          The hash table handle.
 * @param  name        The given key name.
 * @retval The data block pointer.
 */
int32_t xhtable_index(xhal_htable_t *const self, char *name)
{
    xassert_not_null(self);
    xassert_not_null(name);

    uint32_t hash_time33 = _hash_time33(name);
    uint32_t hash_elf    = _hash_elf(name);
    uint32_t hash_bkdr   = _hash_bkdr(name);

    uint32_t index_start = hash_time33 % self->prime_max;
    uint32_t index;
    uint32_t times_count = 0;
    for (uint32_t i = 0; i < self->capacity; i++)
    {
        index = (index_start + i) % self->capacity;
        if (self->table[index].data == NULL)
        {
            continue;
        }
        else if (self->table[index].hash_time33 == hash_time33 &&
                 self->table[index].hash_elf == hash_elf &&
                 self->table[index].hash_bkdr == hash_bkdr)
        {
            return (int32_t)index;
        }
        else
        {
            times_count++;
            if (times_count > XHASH_SEEK_TIMES_MAX)
            {
                break;
            }
        }
    }
#ifdef XDEBUG
    XLOG_ERROR("XHAL_ERROR");
#endif
    return (int32_t)XHAL_ERROR;
}

/**
 * @brief  Get the maximum prime number in the specific range.
 * @param  capacity  The given range size.
 * @retval The maximum prime number.
 */
static uint32_t _get_prime_max(uint32_t capacity)
{
    uint32_t prime_max = 0;

    for (int64_t i = capacity; i > 0; i--)
    {
        uint8_t is_prime = true;
        for (uint32_t j = 2; j < capacity; j++)
        {
            if (i <= j)
            {
                break;
            }
            if ((i % j) == 0)
            {
                is_prime = false;
                break;
            }
        }
        if (is_prime)
        {
            prime_max = i;
            break;
        }
    }

    return prime_max;
}

/**
 * @brief  Calculate the time33 hash value of one string.
 * @param  str  The input string.
 * @retval The time33 hash value.
 */
static uint32_t _hash_time33(char *str)
{
    xassert_not_null(str);

    uint32_t hash = 5381;

    while (*str)
    {
        /* hash *= 33 */
        hash = (*str++) + (hash << 5) + hash;
    }

    return hash;
}

/**
 * @brief  Calculate the ELF hash value of one string.
 * @param  str  The input string.
 * @retval The ELF hash value.
 */
uint32_t _hash_elf(char *str)
{
    xassert_not_null(str);

    uint32_t hash = 0;
    uint32_t x    = 0;

    while (*str)
    {
        hash = (hash << 4) + (*str++);
        if ((x = hash & 0xF0000000L) != 0)
        {
            hash ^= (x >> 24);
            hash &= ~x;
        }
    }

    return (hash & 0x7FFFFFFF);
}

/**
 * @brief  Calculate the BKDR hash value of one string.
 * @param  str  The input string.
 * @retval The BKDR hash value.
 */
uint32_t _hash_bkdr(char *str)
{
    xassert_not_null(str);

    uint32_t seed = 131; /* 31 131 1313 13131 131313 etc.. */
    uint32_t hash = 0;

    while (*str)
    {
        hash = hash * seed + (*str++);
    }

    return (hash & 0x7FFFFFFF);
}
