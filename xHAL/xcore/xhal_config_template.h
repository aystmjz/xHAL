#ifndef __XHAL_CONFIG_H
#define __XHAL_CONFIG_H

#define XHAL_OS_SUPPORTING
//  #define XHAL_UNIT_TEST

#define XHAL_CMSIS_DEVICE_HEADER     "stm32f10x.h"

#define XOS_TICK_RATE_HZ             (1000)

#define XEXPORT_THREAD_STACK_SIZE    (1024)

#define XASSERT_ENABLE               (1)
#define XASSERT_FULL_PATH_ENABLE     (1)

#define XLOG_TIME_MODE               (XLOG_TIME_RELATIVE)

#define XMALLOC_BLOCK_SIZE           (16)
#define XMALLOC_MAX_SIZE             (7 * 1024)

#define XLOG_COLOR_ENABLE            (1)
#define XLOG_NEWLINE_ENABLE          (1)
#define XLOG_TIME_ENABLE             (1)
#define XLOG_DUMP_ENABLE             (1)
#define XLOG_FILEINFO_ENABLE         (1)
#define XLOG_FULL_PATH_ENABLE        (0)
#define XLOG_COMPILE_LEVEL           (XLOG_LEVEL_DEBUG)

#define XTIME_AUTO_SYNC_ENABLE       (1)
#define XTIME_AUTO_SYNC_TIME         (60 * 60 * 60)

#define XSHELL_DEFAULT_USER          "aystmjz"
#define XSHELL_DEFAULT_USER_PASSWORD "pswd"

#endif /* __XHAL_CONFIG_H */
