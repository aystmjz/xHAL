#include "xhal_periph.h"
#include "../xcore/xhal_assert.h"
#include "../xcore/xhal_log.h"
#include "../xcore/xhal_malloc.h"
#include <string.h>

XLOG_TAG("xPeriph");

#ifndef XHAL_PERI_NUM_MAX
#define XHAL_PERI_NUM_MAX (64)
#endif

static xhal_periph_t *xperiph_table[XHAL_PERI_NUM_MAX];
static uint16_t xperiph_count = 0;

#ifdef XHAL_OS_SUPPORTING
static osMutexId_t _xperiph_mutex(void);
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
 * @return 无返回值
 */
void xperiph_register(xhal_periph_t *self, xhal_periph_attr_t *attr)
{
    xassert_not_null(self);
    xassert_not_null(attr);
    xassert_not_null(attr->name);
    xassert_name(xperiph_find(attr->name) == NULL, attr->name);

#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret    = osOK;
    osMutexId_t mutex = _xperiph_mutex();
    ret               = osMutexAcquire(mutex, osWaitForever);
    xassert(ret == osOK);

    self->mutex      = osMutexNew(&xperiph_mutex_attr);
    xassert_not_null(self->mutex);
#endif
    xmemcpy(&self->attr, attr, sizeof(xhal_periph_attr_t));
    self->is_inited = XPERIPH_NOT_INITED;

    xassert_name(xperiph_count < XHAL_PERI_NUM_MAX, attr->name);

    if (xperiph_count == 0)
    {
        for (uint16_t i = 0; i < XHAL_PERI_NUM_MAX; i++)
        {
            xperiph_table[i] = NULL;
        }
    }
    xperiph_table[xperiph_count++] = self;

#ifdef XHAL_OS_SUPPORTING
    ret = osMutexRelease(mutex);
    xassert(ret == osOK);
#endif
}

/**
 * @brief This function unregisters a device with the device handle.
 * @param self   the pointer of device driver structure
 * @retval None.
 */
void xperiph_unregister(xhal_periph_t *self)
{
    xassert_not_null(self);

#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret    = osOK;
    osMutexId_t mutex = _xperiph_mutex();
    ret               = osMutexAcquire(mutex, osWaitForever);
    xassert(ret == osOK);
#endif
    for (uint16_t i = 0; i < XHAL_PERI_NUM_MAX; i++)
    {
        if (xperiph_table[i] == self)
        {

#ifdef XHAL_OS_SUPPORTING
            osStatus_t ret = osMutexDelete(self->mutex);
            xassert(ret == osOK);
            self->mutex = NULL;
#endif
            xperiph_table[i] = NULL;
            xperiph_count--;
            break;
        }
    }

#ifdef XHAL_OS_SUPPORTING
    ret = osMutexRelease(mutex);
    xassert(ret == osOK);
#endif
}

/**
 * @brief Get the count number in device framework management.
 * @retval Count number of devices.
 */
uint16_t xperiph_get_number(void)
{
    uint16_t num = 0;

#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret    = osOK;
    osMutexId_t mutex = _xperiph_mutex();
    ret               = osMutexAcquire(mutex, osWaitForever);
    xassert(ret == osOK);
#endif
    num = xperiph_count;

#ifdef XHAL_OS_SUPPORTING
    ret = osMutexRelease(mutex);
    xassert(ret == osOK);
#endif
    return num;
}

/**
 * @brief 此函数根据指定名称查找设备驱动。
 * @param name    设备名称
 * @return 设备句柄。如果未找到，返回NULL
 */
xhal_periph_t *xperiph_find(const char *name)
{
    xassert_not_null(name);

#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret    = osOK;
    osMutexId_t mutex = _xperiph_mutex();
    ret               = osMutexAcquire(mutex, osWaitForever);
    xassert(ret == osOK);
#endif
    xhal_periph_t *self = NULL;
    for (uint32_t i = 0; i < XHAL_PERI_NUM_MAX; i++)
    {
        if (xperiph_table[i] == NULL)
        {
            break;
        }

        xassert_name(xperiph_table[0]->attr.name != NULL,
                     "Device table corrupted");

        if (strcmp(xperiph_table[i]->attr.name, name) == 0)
        {
            self = xperiph_table[i];
            break;
        }
    }

#ifdef XHAL_OS_SUPPORTING
    ret = osMutexRelease(mutex);
    xassert(ret == osOK);
#endif
    return self;
}

/**
 * @brief 此函数检查设备名称是否有效。
 * @param name    设备名称
 * @return 有效返回真，无效返回假
 */
uint8_t xperiph_valid(const char *name)
{
    return xperiph_find(name) == NULL ? false : true;
}

/**
 * @brief 此函数检查给定名称是否为设备的名称。
 * @param self    设备句柄
 * @param name    设备名称
 * @return 真或假
 */
uint8_t xperiph_of_name(xhal_periph_t *self, const char *name)
{
    uint8_t of_the_name = false;

    xperiph_lock(self);
    if (strcmp(self->attr.name, name) == 0)
    {
        of_the_name = true;
    }
    xperiph_unlock(self);

    return of_the_name;
}

#ifdef XHAL_OS_SUPPORTING
void xperiph_mutex_control(xhal_periph_t *self, uint8_t status)
{
    xassert_not_null(self);
    xassert_not_null(self->mutex);

    osStatus_t ret = osOK;

    if (status)
    {
        ret = osMutexAcquire(self->mutex, osWaitForever);
    }
    else
    {
        ret = osMutexRelease(self->mutex);
    }
    xassert(ret == osOK);
}

static osMutexId_t _xperiph_mutex(void)
{
    if (xperiph_mutex == NULL)
    {
        xperiph_mutex = osMutexNew(&xperiph_mutex_attr);
        xassert_not_null(xperiph_mutex);
    }

    return xperiph_mutex;
}
#endif
