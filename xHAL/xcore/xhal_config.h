#include "xhal_config_user.h"

#ifndef __XHAL_CONFIG_H
#define __XHAL_CONFIG_H

#define FIRMWARE_NAME                "my_firmware_name"
#define HARDWARE_VERSION             "1.0.0"
#define HARDWARE_VERSION_HEX         ((1 << 16) | (0 << 8) | (0))
#define SOFTWARE_VERSION             "1.0.0"
#define SOFTWARE_VERSION_HEX         ((1 << 16) | (0 << 8) | (0))

#define XHAL_DEFAULT_UART            "debug_uart"
#define XHAL_DEVICE_HEADER           "stm32f10x.h"
#define XHAL_CPU_FREQ_HZ             SystemCoreClock

#define XHAL_OS_SUPPORTING           (1)
#define XHAL_UNIT_TEST               (1)
#define XHAL_SHELL                   (1)
#define XHAL_TRACE                   (1)

#define XASSERT_ENABLE               (1)
#define XASSERT_FULL_PATH_ENABLE     (1)
#define XASSERT_FUNC_ENABLE          (1)
#define XASSERT_BACKTRACE_ENABLE     (1)
#define XASSERT_USER_HOOK_ENABLE     (1)

#define XMALLOC_BLOCK_SIZE           (16)
#define XMALLOC_MAX_SIZE             (20 * 1024)

#define XLOG_COLOR_ENABLE            (1)
#define XLOG_NEWLINE_ENABLE          (1)
#define XLOG_FILEINFO_ENABLE         (0)
#define XLOG_FULL_PATH_ENABLE        (0)
#define XLOG_DEFAULT_TIME_MODE       (XLOG_TIME_MOD_RELATIVE)
#define XLOG_DEFAULT_LEVEL           (XLOG_LEVEL_DEBUG)
#define XLOG_COMPILE_LEVEL           (XLOG_LEVEL_DEBUG)

#define XTIME_USE_DWT_DELAY          (0)
#define XTIME_NOP()                  __NOP()

#define XOS_TICK_RATE_HZ             (1000)

#define XSHELL_BUFFER_SIZE           (512)
#define XSHELL_DEFAULT_USER          "user_name"
#define XSHELL_DEFAULT_USER_PASSWORD "pswd"
#define XSHELL_TEXT_INFO                               \
    "\r\n\r\n"                                         \
    "Project Name:   " FIRMWARE_NAME " \r\n"           \
    "HardWare Ver:     " HARDWARE_VERSION "\r\n"       \
    "SoftWare Ver:     " SOFTWARE_VERSION "\r\n"       \
    "Copyright:  (c) 2026 " XSHELL_DEFAULT_USER "\r\n" \
    "Type 'help' to list commands.\r\n"

#define XTRACE_CPU_PLATFORM_TYPE  CMB_CPU_ARM_CORTEX_M3
#define XTRACE_CMB_PRINT_LANGUAGE CMB_PRINT_LANGUAGE_ENGLISH

#define XHAL_VALID_RAM_START      (0x20000000U)
#define XHAL_VALID_RAM_END        (0x2000BFFFU)
#define XHAL_VALID_FLASH_START    (0x08000000U)
#define XHAL_VALID_FLASH_END      (0x08040000U)

#define XHAL_POWER_RESET()        NVIC_SystemReset()
#define XHAL_DISABLE_IRQ()        __disable_irq()
#define XHAL_ENABLE_IRQ()         __enable_irq()

#define xhal_emerg_put_init()     (void)0

#define xhal_emerg_putc(c)        (void)(c)

#define xhal_emerg_puts(s)        (void)(s)

#endif /* __XHAL_CONFIG_H */
