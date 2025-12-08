#ifndef __XHAL_CONFIG_H
#define __XHAL_CONFIG_H

// #define XHAL_OS_SUPPORTING
//  #define XHAL_UNIT_TEST

#define XHAL_DEVICE_HEADER           "xxxxxxxxxx.h"

#define FIRMWARE_NAME                "my_firmware_name"
#define HARDWARE_VERSION             "1.0.0"
#define SOFTWARE_VERSION             "1.0.0"

#define XOS_TICK_RATE_HZ             (1000)

#define XEXPORT_THREAD_STACK_SIZE    (1024)

#define XASSERT_ENABLE               (1)
#define XASSERT_FULL_PATH_ENABLE     (1)
#define XASSERT_FUNC_ENABLE          (1)
#define XASSERT_BACKTRACE_ENABLE     (1)
#define XASSERT_USER_HOOK_ENABLE     (1)

#define XMALLOC_BLOCK_SIZE           (16)
#define XMALLOC_MAX_SIZE             (10 * 1024)

#define XLOG_COLOR_ENABLE            (1)
#define XLOG_NEWLINE_ENABLE          (1)
#define XLOG_FILEINFO_ENABLE         (1)
#define XLOG_FULL_PATH_ENABLE        (0)
#define XLOG_DEFAULT_OUTPUT          my_output
#define XLOG_DEFAULT_TIME_MODE       (XLOG_TIME_MOD_RELATIVE)
#define XLOG_DEFAULT_LEVEL           (XLOG_LEVEL_DEBUG)
#define XLOG_COMPILE_LEVEL           (XLOG_LEVEL_DEBUG)

#define XTIME_USE_DWT_DELAY          (0)
#define XTIME_CPU_FREQ_HZ            (SystemCoreClock)

#define XSHELL_DEFAULT_USER          "user_name"
#define XSHELL_DEFAULT_USER_PASSWORD "pswd"

#define XTRACE_FIRMWARE_NAME         FIRMWARE_NAME
#define XTRACE_HARDWARE_VERSION      HARDWARE_VERSION
#define XTRACE_SOFTWARE_VERSION      SOFTWARE_VERSION

#define XTRACE_OS_PLATFORM_TYPE      CMB_OS_PLATFORM_FREERTOS
#define XTRACE_CPU_PLATFORM_TYPE     CMB_CPU_ARM_CORTEX_M3
#define XTRACE_CMB_PRINT_LANGUAGE    CMB_PRINT_LANGUAGE_ENGLISH

#endif /* __XHAL_CONFIG_H */