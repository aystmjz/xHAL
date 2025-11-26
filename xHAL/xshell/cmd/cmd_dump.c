#include "../../xcore/xhal_common.h"
#include "../../xlib/xhal_bit.h"
#include "../xhal_shell.h"
#include "cmd_config.h"
#include <stdlib.h>
#include <string.h>

#define CMD_DUMP_CDISCRIPTION          \
    "dump <addr> <size> [-a | -e]\r\n" \
    " - a: output ASCII\r\n"           \
    " - e: output ASCII and ESCAPE\r\n"

#define XLOG_DUMP_HEAD_BIT       (0) // flags_mask 中表头的标志位
#define XLOG_DUMP_ASCII_BIT      (1) // flags_mask 中 ASCII 的标志位
#define XLOG_DUMP_ESCAPE_BIT     (2) // flags_mask 中带有转义字符的标志位
#define XLOG_DUMP_TAIL_BIT       (3) // flags_mask 中表尾的标志位

#define XLOG_DUMP_TABLE          (BIT(XLOG_DUMP_HEAD_BIT) | BIT(XLOG_DUMP_TAIL_BIT))
/* 只输出 16 进制格式数据 */
#define XLOG_DUMP_FLAG_HEX_ONLY  (XLOG_DUMP_TABLE)
/* 输出 16 进制格式数据并带有 ASCII 字符 */
#define XLOG_DUMP_FLAG_HEX_ASCII (BIT(XLOG_DUMP_ASCII_BIT) | XLOG_DUMP_TABLE)
/* 输出 16 进制格式数据、 ASCII 字符、转义字符，其余输出 16 进制原始值 */
#define XLOG_DUMP_FLAG_HEX_ASCII_ESCAPE \
    (BIT(XLOG_DUMP_ESCAPE_BIT) | XLOG_DUMP_FLAG_HEX_ASCII)

/* 只输出 16 进制格式数据时，每行实际会输出的字节数 */
#define XLOG_DUMP_HEX_BYTES_PER_LINE              (53)
/* 输出 16 进制格式数据和 ASCII 字符时，每行实际会输出的字节数 */
#define XLOG_DUMP_HEX_ASCII_BYTES_PER_LINE        (72)
/* 输出 16 进制格式数据、 ASCII 字符、转义字符时，每行实际会输出的字节数 */
#define XLOG_DUMP_HEX_ASCII_ESCAPE_BYTES_PER_LINE (104)
/* 表头字节数，可能会多几个字节 */
#define XLOG_DUMP_TABLE_HEADER_BYTES              (332)


#if SHELL_CMD_IS_ENABLED(DUMP)
xhal_err_t dump_memory(Shell *shell, void *addr, xhal_size_t size,
                       uint8_t flags_mask)
{
    if ((!addr) || (size == 0))
    {
        return XHAL_ERR_INVALID;
    }

    if ((!XHAL_IS_VALID_RAM_ADDRESS(addr) &&
         !XHAL_IS_VALID_FLASH_ADDRESS(addr)) ||
        (!XHAL_IS_VALID_RAM_ADDRESS((xhal_pointer_t)addr + size - 1) &&
         !XHAL_IS_VALID_FLASH_ADDRESS((xhal_pointer_t)addr + size - 1)))
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
    shellPrint(shell,
               "MEMORY START ADDRESS: %p, OUTPUT %d BYTES." XHAL_STR_ENTER,
               addr, (int)size);

    /* 打印表头 */
    if (BIT_GET(flags_mask, XLOG_DUMP_HEAD_BIT))
    {
        for (uint8_t i = 0; i < bytes_actually_out_per_line; i++)
        {
            shellPrint(shell, "-");
        }
        shellPrint(shell, XHAL_STR_ENTER);
        shellPrint(shell, " OFS  "); /* 偏移 offset */
        for (uint8_t i = 0; i < bytes_mem_out_per_line; i++)
        {
            shellPrint(shell, "%2X ", i);
        }
        if (BIT_GET(flags_mask, XLOG_DUMP_ASCII_BIT))
        {
            shellPrint(shell, "| ASCII");
        }
        shellPrint(shell, XHAL_STR_ENTER);
        for (uint8_t i = 0; i < bytes_actually_out_per_line; i++)
        {
            shellPrint(shell, "-");
        }
        shellPrint(shell, XHAL_STR_ENTER);
    }

    uint8_t byte = 0; /* 当前地址的字节的值 */
    /* 遍历内存块中 */
    for (xhal_size_t i = 0; i < size; i++)
    {
        byte = *((uint8_t *)addr + i);

        if (i % bytes_mem_out_per_line == 0)
        {
            /* 每行的开头打印行号 */
            shellPrint(shell,
                       "%04lu: ", (unsigned long)(i / bytes_mem_out_per_line));
        }

        shellPrint(shell, "%02X ", byte); /* 打印当前地址的字节的值的十六进制 */

        /* 如果达到每行显示的字节数， */
        /* 或者已经是最后一个字节，就打印 ASCII 字符串 */
        if (((i + 1) % bytes_mem_out_per_line == 0) || (i == size - 1))
        {
            /* 如果不是每行显示的字节数，就补齐空格 */
            if ((i + 1) % bytes_mem_out_per_line != 0)
            {
                for (xhal_size_t j = 0; j < (bytes_mem_out_per_line -
                                             (i + 1) % bytes_mem_out_per_line) *
                                                3;
                     j++)
                {
                    shellPrint(shell, " ");
                }
            }
            /* 遍历输出 ASCII */
            if (BIT_GET(flags_mask, XLOG_DUMP_ASCII_BIT))
            {
                shellPrint(shell, "| ");
                for (xhal_size_t j = i - i % bytes_mem_out_per_line; j <= i;
                     j++)
                {
                    uint8_t b = *((uint8_t *)addr + j);
                    if (b >= ' ' && b <= '~')
                    { /* 可见字符 */
                        if (BIT_GET(flags_mask, XLOG_DUMP_ESCAPE_BIT))
                        {
                            shellPrint(shell, " ");
                        }
                        shellPrint(shell, "%c", b);
                        if (BIT_GET(flags_mask, XLOG_DUMP_ESCAPE_BIT))
                        {
                            shellPrint(shell, " ");
                        }
                    }
                    else if (BIT_GET(flags_mask, XLOG_DUMP_ESCAPE_BIT))
                    {
                        /* 显示转义字符 */
                        switch (b)
                        {
                        case '\0':
                            shellPrint(shell, "\\0 ");
                            break; /* 空字符 */
                        case '\a':
                            shellPrint(shell, "\\a ");
                            break; /* 响铃符 */
                        case '\b':
                            shellPrint(shell, "\\b ");
                            break; /* 退格符 */
                        case '\t':
                            shellPrint(shell, "\\t ");
                            break; /* 水平制表符 */
                        case '\n':
                            shellPrint(shell, "\\n ");
                            break; /* 换行符 */
                        case '\v':
                            shellPrint(shell, "\\v ");
                            break; /* 垂直制表符 */
                        case '\f':
                            shellPrint(shell, "\\f ");
                            break; /* 换页符 */
                        case '\r':
                            shellPrint(shell, "\\r ");
                            break; /* 回车符*/
                        /* 其他不可打印字符，用十六进制表示 */
                        default:
                            shellPrint(shell, "%02x ", b);
                            break;
                        }
                    }
                    else
                    {
                        /* 转义字符外的不可见字符 */
                        shellPrint(shell, ".");
                        if (BIT_GET(flags_mask, XLOG_DUMP_ESCAPE_BIT))
                        {
                            shellPrint(shell, "  ");
                        }
                    } /* else: 非转义字符 */
                } /* for: 输出 ASCII */
            } /* if: 需要输出 ASCII */
            shellPrint(shell, XHAL_STR_ENTER);
        } /* if: 需要输出 ASCII 或空格 */
    } /* for: 0 ~ size */
    /* 表尾 */
    if (BIT_GET(flags_mask, XLOG_DUMP_TAIL_BIT))
    {
        for (uint8_t i = 0; i < bytes_actually_out_per_line; i++)
        {
            shellPrint(shell, "-");
        }
        shellPrint(shell, XHAL_STR_ENTER);
    }

    return XHAL_OK;
}

static int dump_cmd(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    SHELL_ASSERT(shell, return -1);

    if (argc < 3 || argc > 4)
    {
        shellPrint(shell, "usage:\r\n\r\n" CMD_DUMP_CDISCRIPTION);
        return -1;
    }

    void *addr         = (void *)strtoul(argv[1], NULL, 0);
    xhal_size_t size   = (xhal_size_t)strtoul(argv[2], NULL, 0);
    uint8_t flags_mask = XLOG_DUMP_FLAG_HEX_ONLY;

    if (argc == 4)
    {
        if (strcmp(argv[3], "-a") == 0)
        {
            flags_mask = XLOG_DUMP_FLAG_HEX_ASCII;
        }
        else if (strcmp(argv[3], "-e") == 0)
        {
            flags_mask = XLOG_DUMP_FLAG_HEX_ASCII_ESCAPE;
        }
        else
        {
            shellPrint(shell, "unknown parameter: %s\r\n", argv[3]);
            return -1;
        }
    }

    shellPrint(shell, "dump memory: addr=%p, size=%u, flags=0x%02X\r\n\r\n",
               addr, size, flags_mask);

    xhal_err_t ret = dump_memory(shell, addr, size, flags_mask);

    if (ret != XHAL_OK)
    {
        shellPrint(shell, "dump memory failed: %s\r\n", xhal_err_to_str(ret));
        return -1;
    }
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 dump, dump_cmd, "\r\ndump memory\r\n" CMD_DUMP_CDISCRIPTION);
#endif