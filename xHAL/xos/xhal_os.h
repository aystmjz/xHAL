#ifndef __XHAL_OS_H
#define __XHAL_OS_H

#include "xhal_config.h"

#ifdef XHAL_OS_SUPPORTING
#include "cmsis_os/cmsis_os2.h"

#define XOS_MS_TO_TICKS(ms) \
    ((uint32_t)((ms) * osKernelGetTickFreq() / XOS_TICK_RATE_HZ))

#endif

#endif /* __XHAL_OS_H */