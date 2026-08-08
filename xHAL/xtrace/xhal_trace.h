#ifndef __XHAL_TRACE_H
#define __XHAL_TRACE_H

#include "xhal_config.h"

#if (XHAL_TRACE == 1)
    #include "cm_backtrace/cm_backtrace.h"
#endif

#endif /* __XHAL_TRACE_H */