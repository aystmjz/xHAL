#include "xhal_periph.h"
#include "../xcore/xhal_assert.h"
#include "../xcore/xhal_log.h"
#include "../xcore/xhal_malloc.h"
#include <string.h>

XHAL_TAG(xPeriph);

#ifndef XHAL_PERI_NUM_MAX
    #define XHAL_PERI_NUM_MAX (64)
#endif

static xhal_periph_t *volatile xperiph_table[XHAL_PERI_NUM_MAX];
static volatile uint16_t xperiph_count = 0;

static xhal_periph_t *_find_periph(const char *name);

#if (XHAL_OS_SUPPORTING == 1)
osMutexId_t _get_xperiph_mutex(void);
static osMutexId_t xperiph_mutex              = NULL;
static const osMutexAttr_t xperiph_mutex_attr = {
    .name      = "xperiph_mutex",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

/**
 * @brief 此函数使用设备属性注册一个设备。
 * @param self    设备句柄
 * @param attr    设备驱动的属性
 */
xhal_err_t xperiph_register(xhal_periph_t *self, xhal_periph_attr_t *attr)
{
    xassert_not_null(self);
    xassert_not_null(attr);
    xassert_not_null(attr->name);

    xhal_err_t ret   = XHAL_OK;
    uint8_t inserted = 0;

#if (XHAL_OS_SUPPORTING == 1)
    osStatus_t ret_os = osOK;
    osMutexId_t mutex = _get_xperiph_mutex();
    ret_os            = osMutexAcquire(mutex, osWaitForever);
    if (ret_os != osOK)
    {
        return (xhal_err_t)ret_os;
    }

    self->mutex = osMutexNew(&xperiph_mutex_attr);
    if (self->mutex == NULL)
    {
        ret = XHAL_ERROR;
        goto exit;
    }
#endif

    if (_find_periph(attr->name) != NULL)
    {
        ret = XHAL_ERR_EXIST;
        goto exit;
    }

    self->attr      = *attr;
    self->is_inited = XPERIPH_NOT_INITED;

    for (uint16_t i = 0; i < XHAL_PERI_NUM_MAX; i++)
    {
        if (xperiph_table[i] == NULL)
        {
            xperiph_table[i] = self;
            xperiph_count++;
            inserted = 1;
            break;
        }
    }

    if (!inserted)
    {
        ret = XHAL_ERR_NO_MEMORY;
    }

exit:
#if (XHAL_OS_SUPPORTING == 1)
    if ((ret != XHAL_OK) && (self->mutex != NULL))
    {
        osMutexDelete(self->mutex);
        self->mutex = NULL;
    }

    ret_os = osMutexRelease(mutex);
    xassert(ret_os == osOK);
#endif
    return ret;
}

xhal_err_t xperiph_unregister(xhal_periph_t *self)
{
    xassert_not_null(self);

    xhal_err_t ret = XHAL_ERR_NOT_FOUND;

#if (XHAL_OS_SUPPORTING == 1)
    osStatus_t ret_os = osOK;
    osMutexId_t mutex = _get_xperiph_mutex();
    ret_os            = osMutexAcquire(mutex, osWaitForever);
    if (ret_os != osOK)
    {
        return (xhal_err_t)ret_os;
    }
#endif
    for (uint16_t i = 0; i < XHAL_PERI_NUM_MAX; i++)
    {
        if (xperiph_table[i] == self)
        {
#if (XHAL_OS_SUPPORTING == 1)
            osMutexDelete(self->mutex);
            self->mutex = NULL;
#endif
            xperiph_table[i] = NULL;
            xperiph_count--;
            ret = XHAL_OK;
            break;
        }
    }
#if (XHAL_OS_SUPPORTING == 1)
    ret_os = osMutexRelease(mutex);
    xassert(ret_os == osOK);
#endif
    return ret;
}

uint16_t xperiph_get_number(void)
{
    return xperiph_count;
}

xhal_periph_t *xperiph_find(const char *name)
{
    xassert_not_null(name);

    return _find_periph(name);
}

/**
 * @brief 此函数检查设备名称是否有效。
 * @param name    设备名称
 * @return 有效返回真，无效返回假
 */
bool xperiph_valid(const char *name)
{
    return xperiph_find(name) == NULL ? false : true;
}

/**
 * @brief 此函数检查给定名称是否为设备的名称。
 * @param self    设备句柄
 * @param name    设备名称
 * @return 真或假
 */
bool xperiph_of_name(xhal_periph_t *self, const char *name)
{
    xassert_not_null(self);
    xassert_not_null(name);

    bool ret = false;

    xperiph_lock(self);
    if (self->attr.name != NULL && strcmp(self->attr.name, name) == 0)
    {
        ret = true;
    }
    xperiph_unlock(self);

    return ret;
}

static xhal_periph_t *_find_periph(const char *name)
{
    for (uint32_t i = 0; i < XHAL_PERI_NUM_MAX; i++)
    {
        if (xperiph_table[i] == NULL || xperiph_table[i]->attr.name == NULL)
        {
            continue;
        }

        if (strcmp(xperiph_table[i]->attr.name, name) == 0)
        {
            return xperiph_table[i];
        }
    }

    return NULL;
}

#if (XHAL_OS_SUPPORTING == 1)
void xperiph_mutex_control(xhal_periph_t *self, uint8_t status)
{
    xassert_not_null(self);
    xassert_not_null(self->mutex);

    osStatus_t ret_os = osOK;

    if (status)
    {
        ret_os = osMutexAcquire(self->mutex, osWaitForever);
    }
    else
    {
        ret_os = osMutexRelease(self->mutex);
    }
    xassert(ret_os == osOK);
}

osMutexId_t _get_xperiph_mutex(void)
{
    if (xperiph_mutex == NULL)
    {
        xperiph_mutex = osMutexNew(&xperiph_mutex_attr);
    }

    return xperiph_mutex;
}
#endif
