#ifndef __XHAL_PERIPH_H
#define __XHAL_PERIPH_H

#include "../xcore/xhal_def.h"
#include "../xos/xhal_os.h"
#include "xhal_config.h"

#define XPERIPH_INITED     1
#define XPERIPH_NOT_INITED 0

enum xhal_periph_type
{
    XHAL_PERIPH_NULL = 0,

    XHAL_PERIPH_PIN,
    XHAL_PERIPH_PWM,
    XHAL_PERIPH_ADC,
    XHAL_PERIPH_DAC,
    XHAL_PERIPH_UART,
    XHAL_PERIPH_I2C_BUS,
    XHAL_PERIPH_I2C,
    XHAL_PERIPH_SPI_BUS,
    XHAL_PERIPH_SPI,
    XHAL_PERIPH_CAN,
    XHAL_PERIPH_WATCHDOG,
    XHAL_PERIPH_RTC,
    XHAL_PERIPH_UNKNOWN,

    XHAL_PERIPH_NORMAL_MAX,
};

typedef struct xhal_periph_attr
{
    const char *name;
    uint8_t type;
} xhal_periph_attr_t;

typedef struct xhal_periph
{
    xhal_periph_attr_t attr;

#ifdef XHAL_OS_SUPPORTING
    uint8_t lock_count;
    osMutexId_t mutex;
#endif

    uint8_t is_inited;

} xhal_periph_t;

#define XPERIPH_CAST(_peri) ((xhal_periph_t *)_peri)

#define XPERIPH_CHECK_INIT(_peri, _return)                        \
    do                                                            \
    {                                                             \
        if (XPERIPH_CAST(_peri)->is_inited == XPERIPH_NOT_INITED) \
        {                                                         \
            return _return;                                       \
        }                                                         \
    } while (0)

#define XPERIPH_CHECK_TYPE(_peri, _type) \
    xassert_name(_peri->attr.type == _type, _peri->attr.name)

void xperiph_register(xhal_periph_t *self, xhal_periph_attr_t *attr);
void xperiph_unregister(xhal_periph_t *self);
uint16_t xperiph_get_number(void);
xhal_periph_t *xperiph_find(const char *name);
uint8_t xperiph_valid(const char *name);
uint8_t xperiph_of_name(xhal_periph_t *self, const char *name);

#ifdef XHAL_OS_SUPPORTING
void xperiph_mutex_control(xhal_periph_t *self, uint8_t status);
/**
 * @brief 锁定设备以确保线程安全性。
 * @param _peri    设备句柄
 * @return 无返回值
 */
#define xperiph_lock(_peri)   xperiph_mutex_control(XPERIPH_CAST(_peri), true)

/**
 * @brief 解锁设备以确保线程安全性。
 * @param _peri    设备句柄
 * @return 无返回值
 */
#define xperiph_unlock(_peri) xperiph_mutex_control(XPERIPH_CAST(_peri), false)

#else

#define xperiph_lock(_peri)
#define xperiph_unlock(_peri)

#endif

#endif /* __XHAL_PERIPH_H */
