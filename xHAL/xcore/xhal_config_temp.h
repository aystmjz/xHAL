#ifndef __XHAL_CONFIG_H
#define __XHAL_CONFIG_H

#define XHAL_OS_SUPPORTING
//  #define XHAL_UNIT_TEST

#define XHAL_CMSIS_DEVICE_HEADER     "stm32f10x.h"

#define FIRMWARE_NAME                "Smart-Wake-Up-Light"
#define HARDWARE_VERSION             "1.1.1"
#define SOFTWARE_VERSION             "1.2.0"

#define XOS_TICK_RATE_HZ             (1000)

#define XEXPORT_THREAD_STACK_SIZE    (1524)

#define XASSERT_ENABLE               (1)
#define XASSERT_FULL_PATH_ENABLE     (1)
#define XASSERT_FUNC_ENABLE          (1)
#define XASSERT_BACKTRACE_ENABLE     (1)
#define XASSERT_USER_HOOK_ENABLE     (1)

#define XLOG_TIME_MODE               (XLOG_TIME_RELATIVE)

#define XMALLOC_BLOCK_SIZE           (16)
#define XMALLOC_MAX_SIZE             (8 * 1024)

#define XLOG_COLOR_ENABLE            (1)
#define XLOG_NEWLINE_ENABLE          (1)
#define XLOG_TIME_ENABLE             (1)
#define XLOG_FILEINFO_ENABLE         (1)
#define XLOG_FULL_PATH_ENABLE        (0)
#define XLOG_DEFAULT_OUTPUT          xlog_output
#define XLOG_COMPILE_LEVEL           (XLOG_LEVEL_DEBUG)

#define XTIME_AUTO_SYNC_ENABLE       (1)
#define XTIME_AUTO_SYNC_TIME         (60 * 60 * 60)
#define XTIME_CPU_FREQ_HZ            (SystemCoreClock)

#define XSHELL_DEFAULT_USER          "aystmjz"
#define XSHELL_DEFAULT_USER_PASSWORD "pswd"

#define XTRACE_FIRMWARE_NAME         FIRMWARE_NAME
#define XTRACE_HARDWARE_VERSION      HARDWARE_VERSION
#define XTRACE_SOFTWARE_VERSION      SOFTWARE_VERSION

#define XTRACE_OS_PLATFORM_TYPE      CMB_OS_PLATFORM_FREERTOS
#define XTRACE_CPU_PLATFORM_TYPE     CMB_CPU_ARM_CORTEX_M3
#define XTRACE_CMB_PRINT_LANGUAGE    CMB_PRINT_LANGUAGE_ENGLISH

#endif /* __XHAL_CONFIG_H */