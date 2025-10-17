#include "xhal_log.h"
#include "xhal_time.h"
#include <stdio.h>

#define XLOG_BUFF_SIZE     (256)  /* 日志缓冲区大小 */
#define XLOG_DEFAULT_PUTS  (puts) /* 默认日志输出函数 */

#define XLOG_TIME_MILLIS   (1)
#define XLOG_TIME_RELATIVE (2)
#define XLOG_TIME_ABSOLUTE (3)

#ifndef XLOG_COLOR_ENABLE
#define XLOG_COLOR_ENABLE (1) /* 日志颜色显示 */
#endif

#ifndef XLOG_NEWLINE_ENABLE
#define XLOG_NEWLINE_ENABLE (1) /* 日志换行 */
#endif

#ifndef XLOG_TIME_ENABLE
#define XLOG_TIME_ENABLE (1)
#endif

#ifndef XLOG_TIME_MODE
#define XLOG_TIME_MODE (XLOG_TIME_ABSOLUTE)
#endif

/* 终端颜色控制序列 */
#define NONE       "\033[0;0m"  /* 默认颜色 */
#define LIGHT_RED  "\033[1;31m" /* 浅红色 */
#define YELLOW     "\033[0;33m" /* 黄色 */
#define LIGHT_BLUE "\033[1;34m" /* 浅蓝色 */
#define GREEN      "\033[0;32m" /* 绿色 */

#ifdef XHAL_OS_SUPPORTING
#include "../xos/xhal_os.h"

static osMutexId_t _xlog_mutex(void);

static char buff[XLOG_BUFF_SIZE];

static osMutexId_t xlog_mutex              = NULL;
static const osMutexAttr_t xlog_mutex_attr = {
    .name      = "xlog_mutex",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

#if (XLOG_COLOR_ENABLE != 0)
static const char *const XLOG_color_table[XLOG_LEVEL_MAX] = {
    NONE, LIGHT_RED, YELLOW, LIGHT_BLUE, GREEN,
};
#endif

static const char XLOG_level_lable[XLOG_LEVEL_MAX] = {
    ' ', 'E', 'W', 'I', 'D',
};

static uint8_t XLOG_level           = XLOG_LEVEL_DEBUG;
static xlog_output_str_t _xlog_puts = (xlog_output_str_t)XLOG_DEFAULT_PUTS;

void xlog_puts(const char *s)
{
    _xlog_puts(s);
}

void xlog_putc(char c)
{
    char str[2];
    str[0] = c;
    str[1] = '\0';
    _xlog_puts(str);
}

/**
 * @brief 设置日志输出函数
 * @param func 新的日志输出函数指针，如果为NULL则使用默认输出函数
 */
void xlog_set_puts_func(xlog_output_str_t func)
{
    if (func != NULL)
        _xlog_puts = func;
    else
        _xlog_puts = (xlog_output_str_t)XLOG_DEFAULT_PUTS;
}

/**
 * @brief 设置日志级别
 * @param level 新的日志级别
 */
void xlog_set_level(uint8_t level)
{
    XLOG_level = level;
    if (XLOG_level >= XLOG_LEVEL_MAX)
    {
        XLOG_level = XLOG_LEVEL_MAX - 1;
    }
}

xhal_err_t xlog_printf(const char *fmt, ...)
{
    if (fmt == NULL)
    {
        return XHAL_ERR_INVALID;
    }

    xhal_err_t ret;

#ifdef XHAL_OS_SUPPORTING
    osMutexId_t mutex = _xlog_mutex();
    osMutexAcquire(mutex, osWaitForever);
#else
    char buff[XLOG_BUFF_SIZE];
#endif

    va_list args;
    va_start(args, fmt);

    int32_t count = vsnprintf(buff, sizeof(buff), fmt, args);

    va_end(args);

    if (count < 0)
    {
        return XHAL_ERR_INVALID;
    }

    if (count >= sizeof(buff))
    {
        buff[sizeof(buff) - 1] = '\0';
        ret                    = XHAL_ERR_NO_MEMORY;
    }
    else
    {
        ret = XHAL_OK;
    }

    _xlog_puts(buff);

#ifdef XHAL_OS_SUPPORTING
    osMutexRelease(mutex);
#endif

    return ret;
}

xhal_err_t xlog_print_log(const char *name, uint8_t level, const char *fmt, ...)
{

    if (fmt == NULL || name == NULL)
    {
        return XHAL_ERR_INVALID;
    }

    if (XLOG_level < level)
        return XHAL_OK;

#ifdef XHAL_OS_SUPPORTING
    osMutexId_t mutex = _xlog_mutex();
    osMutexAcquire(mutex, osWaitForever);
#else
    char buff[XLOG_BUFF_SIZE];
#endif

    int32_t offset = 0;

#if XLOG_TIME_MODE == XLOG_TIME_MILLIS
    char str_buff[sizeof("XXXXXXXXXXX")];
    snprintf(str_buff, sizeof(str_buff), "%04llums", xtime_get_tick_ms());
#elif XLOG_TIME_MODE == XLOG_TIME_RELATIVE
    char str_buff[sizeof("HHHHH:MM:SS.XXX")];
    xtime_get_format_uptime(str_buff, sizeof(str_buff));
#elif XLOG_TIME_MODE == XLOG_TIME_ABSOLUTE
    char str_buff[sizeof("YYYY-MM-DD HH:MM:SS")];
    xhal_err_t ret = xtime_get_format_time(str_buff, sizeof(str_buff));
    if (ret != XHAL_OK)
    {
        snprintf(str_buff, sizeof(str_buff), "%04llums", xtime_get_tick_ms());
    }
#endif

#if (XLOG_TIME_MODE == XLOG_TIME_MILLIS) ||   \
    (XLOG_TIME_MODE == XLOG_TIME_RELATIVE) || \
    (XLOG_TIME_MODE == XLOG_TIME_ABSOLUTE)

#if XLOG_COLOR_ENABLE != 0
    offset =
        snprintf(buff, XLOG_BUFF_SIZE, "%s[%c/%s %s] ", XLOG_color_table[level],
                 XLOG_level_lable[level], name, str_buff);
#else
    offset = snprintf(buff, XLOG_BUFF_SIZE, "[%c/%s %s] ",
                      XLOG_level_lable[level], name, str_buff);
#endif /* XLOG_COLOR_ENABLE != 0 */

#else

#if XLOG_COLOR_ENABLE != 0
    offset = snprintf(buff, XLOG_BUFF_SIZE, "%s[%c/%s] ",
                      XLOG_color_table[level], XLOG_level_lable[level], name);
#else
    offset = snprintf(buff, XLOG_BUFF_SIZE, "[%c/%s] ", XLOG_level_lable[level],
                      name);
#endif /* XLOG_COLOR_ENABLE != 0 */

#endif /* XLOG_TIME_MODE */

    if (offset < 0 || offset >= XLOG_BUFF_SIZE)
    {
        return XHAL_ERR_NO_MEMORY;
    }

    va_list args;
    va_start(args, fmt);
    int32_t count =
        vsnprintf(buff + offset, XLOG_BUFF_SIZE - offset, fmt, args);
    va_end(args);

    if (count < 0)
    {
        return XHAL_ERR_INVALID;
    }

    if (count >= (XLOG_BUFF_SIZE - offset))
    {
        return XHAL_ERR_NO_MEMORY;
    }

    _xlog_puts(buff);

#if (XLOG_NEWLINE_ENABLE != 0)
    _xlog_puts(XHAL_STR_ENTER);
#endif

#if (XLOG_COLOR_ENABLE != 0)
    _xlog_puts(NONE); /* 重置颜色 */
#endif

#ifdef XHAL_OS_SUPPORTING
    osMutexRelease(mutex);
#endif

    return XHAL_OK;
}

#if XLOG_DUMP_ENABLE != 0
#include "../xlib/xhal_bit.h"

/* 只输出 16 进制格式数据时，每行实际会输出的字节数 */
#define XLOG_DUMP_HEX_BYTES_PER_LINE              (53)
/* 输出 16 进制格式数据和 ASCII 字符时，每行实际会输出的字节数 */
#define XLOG_DUMP_HEX_ASCII_BYTES_PER_LINE        (72)
/* 输出 16 进制格式数据、 ASCII 字符、转义字符时，每行实际会输出的字节数 */
#define XLOG_DUMP_HEX_ASCII_ESCAPE_BYTES_PER_LINE (104)
/* 表头字节数，可能会多几个字节 */
#define XLOG_DUMP_TABLE_HEADER_BYTES              (332)

/**
 * @brief 输出内存信息。
 *
 * @param addr 内存地址。
 * @param size 待输出的内存字节长度。
 * @param flags_mask 格式掩码，见 XLOG_DUMP_FLAG_*。
 * @return xf_err_t
 *      - XF_ERR_INVALID_ARG            参数错误
 *      - XF_OK                         成功
 */
xhal_err_t xlog_dump_mem(void *addr, xhal_size_t size, uint8_t flags_mask)
{
    if ((!addr) || (size == 0))
    {
        return XHAL_ERR_INVALID;
    }

    /* 每行输出的内存字节数，不等于实际输出字节 */
    const uint8_t bytes_mem_out_per_line = 16;

    /* 每行实际输出字节数 */
    uint8_t bytes_actually_out_per_line = 0;
    if (!BIT_GET(flags_mask, XLOG_DUMP_ASCII_BIT))
    {
        bytes_actually_out_per_line = XLOG_DUMP_HEX_BYTES_PER_LINE;
    }
    else if (BIT_GET(flags_mask, XLOG_DUMP_ASCII_BIT) &&
             !BIT_GET(flags_mask, XLOG_DUMP_ESCAPE_BIT))
    {
        bytes_actually_out_per_line = XLOG_DUMP_HEX_ASCII_BYTES_PER_LINE;
    }
    else if (BIT_GET(flags_mask, XLOG_DUMP_ASCII_BIT) &&
             BIT_GET(flags_mask, XLOG_DUMP_ESCAPE_BIT))
    {
        bytes_actually_out_per_line = XLOG_DUMP_HEX_ASCII_ESCAPE_BYTES_PER_LINE;
    }

    /* 打印内存块的起始地址 */
    xlog_printf("MEMORY START ADDRESS: %p, OUTPUT %d BYTES." XHAL_STR_ENTER,
                addr, (int)size);

    /* 打印表头 */
    if (BIT_GET(flags_mask, XLOG_DUMP_HEAD_BIT))
    {
        for (uint8_t i = 0; i < bytes_actually_out_per_line; i++)
        {
            xlog_printf("-");
        }
        xlog_printf(XHAL_STR_ENTER);
        xlog_printf(" OFS  "); /* 偏移 offset */
        for (uint8_t i = 0; i < bytes_mem_out_per_line; i++)
        {
            xlog_printf("%2X ", i);
        }
        if (BIT_GET(flags_mask, XLOG_DUMP_ASCII_BIT))
        {
            xlog_printf("| ASCII");
        }
        xlog_printf(XHAL_STR_ENTER);
        for (uint8_t i = 0; i < bytes_actually_out_per_line; i++)
        {
            xlog_printf("-");
        }
        xlog_printf(XHAL_STR_ENTER);
    }

    uint8_t byte = 0; /* 当前地址的字节的值 */
    /* 遍历内存块中 */
    for (size_t i = 0; i < size; i++)
    {
        byte = *((uint8_t *)addr + i);

        if (i % bytes_mem_out_per_line == 0)
        {
            /* 每行的开头打印行号 */
            xlog_printf("%04lu: ", (unsigned long)(i / bytes_mem_out_per_line));
        }

        xlog_printf("%02X ", byte); /* 打印当前地址的字节的值的十六进制 */

        /* 如果达到每行显示的字节数， */
        /* 或者已经是最后一个字节，就打印 ASCII 字符串 */
        if (((i + 1) % bytes_mem_out_per_line == 0) || (i == size - 1))
        {
            /* 如果不是每行显示的字节数，就补齐空格 */
            if ((i + 1) % bytes_mem_out_per_line != 0)
            {
                for (size_t j = 0; j < (bytes_mem_out_per_line -
                                        (i + 1) % bytes_mem_out_per_line) *
                                           3;
                     j++)
                {
                    xlog_printf(" ");
                }
            }
            /* 遍历输出 ASCII */
            if (BIT_GET(flags_mask, XLOG_DUMP_ASCII_BIT))
            {
                xlog_printf("| ");
                for (size_t j = i - i % bytes_mem_out_per_line; j <= i; j++)
                {
                    uint8_t b = *((uint8_t *)addr + j);
                    if (b >= ' ' && b <= '~')
                    { /* 可见字符 */
                        if (BIT_GET(flags_mask, XLOG_DUMP_ESCAPE_BIT))
                        {
                            xlog_printf(" ");
                        }
                        xlog_printf("%c", b);
                        if (BIT_GET(flags_mask, XLOG_DUMP_ESCAPE_BIT))
                        {
                            xlog_printf(" ");
                        }
                    }
                    else if (BIT_GET(flags_mask, XLOG_DUMP_ESCAPE_BIT))
                    {
                        /* 显示转义字符 */
                        switch (b)
                        {
                        case '\0':
                            xlog_printf("\\0 ");
                            break; /* 空字符 */
                        case '\a':
                            xlog_printf("\\a ");
                            break; /* 响铃符 */
                        case '\b':
                            xlog_printf("\\b ");
                            break; /* 退格符 */
                        case '\t':
                            xlog_printf("\\t ");
                            break; /* 水平制表符 */
                        case '\n':
                            xlog_printf("\\n ");
                            break; /* 换行符 */
                        case '\v':
                            xlog_printf("\\v ");
                            break; /* 垂直制表符 */
                        case '\f':
                            xlog_printf("\\f ");
                            break; /* 换页符 */
                        case '\r':
                            xlog_printf("\\r ");
                            break; /* 回车符*/
                        /* 其他不可打印字符，用十六进制表示 */
                        default:
                            xlog_printf("%02x ", b);
                            break;
                        }
                    }
                    else
                    {
                        /* 转义字符外的不可见字符 */
                        xlog_printf(".");
                        if (BIT_GET(flags_mask, XLOG_DUMP_ESCAPE_BIT))
                        {
                            xlog_printf("  ");
                        }
                    } /* else: 非转义字符 */
                } /* for: 输出 ASCII */
            } /* if: 需要输出 ASCII */
            xlog_printf(XHAL_STR_ENTER);
        } /* if: 需要输出 ASCII 或空格 */
    } /* for: 0 ~ size */
    /* 表尾 */
    if (BIT_GET(flags_mask, XLOG_DUMP_TAIL_BIT))
    {
        for (uint8_t i = 0; i < bytes_actually_out_per_line; i++)
        {
            xlog_printf("-");
        }
        xlog_printf(XHAL_STR_ENTER);
    }

    return XHAL_OK;
}
#endif

#ifdef XHAL_OS_SUPPORTING
static osMutexId_t _xlog_mutex(void)
{
    if (xlog_mutex == NULL)
    {
        xlog_mutex = osMutexNew(&xlog_mutex_attr);
    }

    return xlog_mutex;
}
#endif
