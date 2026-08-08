#include "../../xcore/xhal_malloc.h"
#include "../../xlib/xhal_htable.h"
#include "../xhal_test.h"
#include "test_config.h"
#include <stdlib.h>
#include <string.h>

// #define PRINTF TEST_PRINTF

#ifndef PRINTF
    #define PRINTF(...)
#endif

/* Private config ------------------------------------------------------------*/
#define UT_HASH_TABLE_SIZE_MAX   (128)
#define UT_HASH_TABLE_SIZE       (64)
#define UT_STRING_LENGTH         (16)
#define UT_HASH_TABLE_TEST_TIMES (10)

/* Private function prototypes -----------------------------------------------*/
static void _random_string_generate(char *string, uint32_t size);
static uint16_t _get_max_prime(uint32_t size);
static void _put_string_into_table(char *str);
static xhal_err_t _get_random_string_from_table(char *str);

/* Private variables ---------------------------------------------------------*/
static char *str_table[UT_HASH_TABLE_SIZE];
static uint32_t *payload           = NULL;
static xhal_htable_t *xhal_htable  = NULL;
static xhal_htable_data_t *ht_data = NULL;
static char *str_temp              = NULL;
static char *str_change            = NULL;

static const uint16_t prime_table[] = {
    2,   3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,  41,  43,
    47,  53,  59,  61,  67,  71,  73,  79,  83,  89,  97,  101, 103, 107,
    109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181,
    191, 193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263,
    269, 271, 277, 281, 283, 293, 307, 311, 313, 317, 331, 337, 347, 349,
    353, 359, 367, 373, 379, 383, 389, 397, 401, 409, 419, 421, 431, 433,
    439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503, 509, 521,
    523, 541, 547, 557, 563, 569, 571, 577, 587, 593, 599, 601, 607, 613,
    617, 619, 631, 641, 643, 647, 653, 659, 661, 673, 677, 683, 691, 701,
    709, 719, 727, 733, 739, 743, 751, 757, 761, 769, 773, 787, 797, 809,
    811, 821, 823, 827, 829, 839, 853, 857, 859, 863, 877, 881, 883, 887,
    907, 911, 919, 929, 937, 941, 947, 953, 967, 971, 977, 983, 991, 997};

/* Exported functions --------------------------------------------------------*/

#if TEST_IS_ENABLED(HASH_TABLE)
TEST_GROUP(xhal_htable);

TEST_SETUP(xhal_htable)
{
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(UT_HASH_TABLE_SIZE_MAX,
                                     UT_HASH_TABLE_SIZE);

    for (uint32_t i = 0; i < UT_HASH_TABLE_SIZE; i++)
    {
        str_table[i] = xmalloc(UT_STRING_LENGTH);
        TEST_ASSERT_NOT_NULL(str_table[i]);
        memset(str_table[i], 0, UT_STRING_LENGTH);
    }

    payload = xmalloc(UT_HASH_TABLE_TEST_TIMES * sizeof(uint32_t));
    TEST_ASSERT_NOT_NULL(payload);

    str_temp = xmalloc(UT_STRING_LENGTH);
    TEST_ASSERT_NOT_NULL(str_temp);

    str_change = xmalloc(UT_STRING_LENGTH);
    TEST_ASSERT_NOT_NULL(str_change);

    srand(xtime_get_ts());

    PRINTF("\r\nTest setup done. Allocated buffers for %d strings, payload "
           "size %d",
           UT_HASH_TABLE_SIZE, UT_HASH_TABLE_TEST_TIMES);
}

TEST_TEAR_DOWN(xhal_htable)
{
    for (uint32_t i = 0; i < UT_HASH_TABLE_SIZE; i++)
    {
        xfree(str_table[i]);
    }
    xfree(payload);
    xfree(str_temp);
    xfree(str_change);

    PRINTF("Test teardown complete. Freed all allocated buffers.");
}

TEST(xhal_htable, init)
{
    xhal_htable = xmalloc(sizeof(xhal_htable_t));
    TEST_ASSERT_NOT_NULL(xhal_htable);
    for (uint32_t i = 10; i < UT_HASH_TABLE_SIZE_MAX; i++)
    {
        ht_data = xmalloc(i * sizeof(xhal_htable_data_t));
        TEST_ASSERT_NOT_NULL(ht_data);

        xhtable_init(xhal_htable, ht_data, i);
        PRINTF("Hashtable init: capacity=%d, prime_max=%d",
               xhal_htable->capacity, xhal_htable->prime_max);

        TEST_ASSERT_EQUAL_UINT32(i, xhal_htable->capacity);
        TEST_ASSERT_EQUAL_UINT32(_get_max_prime(i), xhal_htable->prime_max);

        xfree(ht_data);
    }
    xfree(xhal_htable);

    for (uint32_t i = 10; i < UT_HASH_TABLE_SIZE_MAX; i++)
    {
        xhal_htable = xhtable_new(i);
        PRINTF("Hashtable new: capacity=%d, prime_max=%d",
               xhal_htable->capacity, xhal_htable->prime_max);

        TEST_ASSERT_EQUAL_UINT32(i, xhal_htable->capacity);
        TEST_ASSERT_EQUAL_UINT32(_get_max_prime(i), xhal_htable->prime_max);
        xhtable_destroy(xhal_htable);
    }
}

TEST(xhal_htable, random_add_remove)
{
    xhal_err_t ret;
    uint32_t count_fill   = 0;
    uint32_t count_remove = 0;

    for (uint32_t i = 0; i < UT_HASH_TABLE_TEST_TIMES; i++)
    {
        payload[i] = i;
    }

    xhal_htable_t *ht = xhtable_new(UT_HASH_TABLE_SIZE);
    PRINTF("Hashtable created for random add/remove test. capacity=%d",
           UT_HASH_TABLE_SIZE);

    uint8_t fill = true;
    while (1)
    {
        fill = ((rand() % 2) == 0) ? false : true;

        if (fill && (count_fill < UT_HASH_TABLE_TEST_TIMES))
        {
            _random_string_generate(str_temp, UT_STRING_LENGTH);
            if (xhtable_add(ht, str_temp, &payload[count_fill]) == XHAL_OK)
            {
                PRINTF("Add key=%s, value=%u", str_temp, payload[count_fill]);
                _put_string_into_table(str_temp);
                count_fill++;
            }
        }

        if (!fill && (count_remove < UT_HASH_TABLE_TEST_TIMES))
        {
            ret = _get_random_string_from_table(str_temp);
            if (ret == XHAL_OK)
            {
                PRINTF("Try remove key=%s", str_temp);
                ret = xhtable_remove(ht, str_temp);
                PRINTF("Remove result for key=%s: %s", str_temp,
                       (ret == XHAL_OK) ? "OK" : "FAIL");
                count_remove++;
            }
        }

        if ((count_fill >= UT_HASH_TABLE_TEST_TIMES) &&
            (count_remove >= UT_HASH_TABLE_TEST_TIMES))
        {
            break;
        }
    }

    xhtable_destroy(ht);
    PRINTF("Hashtable destroyed after random add/remove test.");
}

TEST_GROUP_RUNNER(xhal_htable)
{
    RUN_TEST_CASE(xhal_htable, init);
    RUN_TEST_CASE(xhal_htable, random_add_remove);
}
#endif

/* Private helper functions --------------------------------------------------*/
static void _put_string_into_table(char *str)
{
    for (uint32_t i = 0; i < UT_HASH_TABLE_SIZE; i++)
    {
        if (str_table[i][0] == 0)
        {
            strcpy(str_table[i], str);
            PRINTF("Put string into table[%d]=%s", i, str);
            break;
        }
    }
}

static xhal_err_t _get_random_string_from_table(char *str)
{
    xhal_err_t ret       = XHAL_ERROR;
    uint32_t index_start = rand() % UT_HASH_TABLE_SIZE;
    uint32_t index       = 0;

    for (uint32_t i = 0; i < UT_HASH_TABLE_SIZE; i++)
    {
        index = (index_start + i) % UT_HASH_TABLE_SIZE;
        if (str_table[index][0] != 0)
        {
            strcpy(str, str_table[index]);
            memset(str_table[index], 0, UT_STRING_LENGTH);
            ret = XHAL_OK;
            PRINTF("Get random string from table[%d]=%s", index, str);
            break;
        }
    }

    return ret;
}

static void _random_string_generate(char *str, uint32_t size)
{
    uint32_t length = 0;
    while (1)
    {
        length = rand() % size;
        if (length > (size / 4) && length < size)
        {
            break;
        }
    }

    memset(str, 0, size);
    for (uint32_t i = 0; i < length;)
    {
        char ch = rand() % 128;
        if (ch >= '0' && ch <= 'z')
        {
            str[i++] = ch;
        }
    }

    PRINTF("Generated random string=%s (len=%d)", str, length);
}

static uint16_t _get_max_prime(uint32_t size)
{
    TEST_ASSERT(size <= UT_HASH_TABLE_SIZE_MAX);

    uint16_t prime = prime_table[0];
    for (uint32_t i = 0; i < sizeof(prime_table) / sizeof(uint16_t); i++)
    {
        if (size < prime_table[i])
        {
            break;
        }
        else
        {
            prime = prime_table[i];
        }
    }

    return prime;
}
