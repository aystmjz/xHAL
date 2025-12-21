#ifndef __XENCODER_H
#define __XENCODER_H

#include "xhal_def.h"
#include "xhal_os.h"

typedef struct xencoder xencoder_t;

typedef struct xencoder_ops
{
    xhal_err_t (*get_position)(xencoder_t *self, int16_t *position);
    xhal_err_t (*clear)(xencoder_t *self);
} xencoder_ops_t;

typedef struct xencoder
{
    int16_t last_position;
    const xencoder_ops_t *ops;

#ifdef XHAL_OS_SUPPORTING
    osMutexId_t mutex;
#endif
} xencoder_t;

xhal_err_t xencoder_init(xencoder_t *encoder, const xencoder_ops_t *ops);
xhal_err_t xencoder_deinit(xencoder_t *encoder);

xhal_err_t xencoder_get_position(xencoder_t *encoder, int16_t *position);
xhal_err_t xencoder_get_delta(xencoder_t *encoder, int16_t *delta);
xhal_err_t xencoder_clear(xencoder_t *encoder);

#endif /* __XENCODER_H */
