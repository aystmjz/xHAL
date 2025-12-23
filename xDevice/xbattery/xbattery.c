#include "xbattery.h"
#include "xhal_assert.h"
#include "xhal_log.h"
#include "xhal_malloc.h"
#include <string.h>

XLOG_TAG("xBATTERY");

#ifdef XHAL_OS_SUPPORTING
static const osMutexAttr_t xbattery_mutex_attr = {
    .name      = "xbattery_mutex",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

static inline void _lock(xbattery_t *battery);
static inline void _unlock(xbattery_t *battery);

static const uint16_t voltage_lookup_table[101];
static void _update_battery_state(xbattery_t *battery);
static void _calculate_percentage(xbattery_t *battery);
static void _update_voltage_buffer(xbattery_t *battery, uint16_t voltage_mv);
static uint16_t _get_average_voltage(xbattery_t *battery);

xhal_err_t xbattery_init(xbattery_t *battery, xbattery_config_t *config)
{
    xassert_not_null(battery);

    xmemset(battery->voltage_mv, 0, sizeof(battery->voltage_mv));
    battery->is_init = 1;

#ifdef XHAL_OS_SUPPORTING
    battery->mutex = osMutexNew(&xbattery_mutex_attr);
    xassert_not_null(battery->mutex);
#endif

    xhal_err_t ret = xbattery_set_config(battery, config);
    if (ret != XHAL_OK)
    {
#ifdef XHAL_OS_SUPPORTING
        osMutexDelete(battery->mutex);
#endif
        return ret;
    }

    return XHAL_OK;
}

xhal_err_t xbattery_deinit(xbattery_t *battery)
{
    xassert_not_null(battery);

    if (!battery->is_init)
    {
        return XHAL_ERR_NO_INIT;
    }

    battery->is_init = 0;

#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osMutexDelete(battery->mutex);
    if (ret_os != osOK)
    {
        return XHAL_ERROR;
    }
    battery->mutex = NULL;
#endif

    return XHAL_OK;
}

xhal_err_t xbattery_update(xbattery_t *battery, uint16_t voltage_mv)
{
    xassert_not_null(battery);

    if (!battery->is_init)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(battery);
    _update_voltage_buffer(battery, voltage_mv);
    _calculate_percentage(battery);
    _update_battery_state(battery);
    _unlock(battery);

    return XHAL_OK;
}

xhal_err_t xbattery_get_state(xbattery_t *battery, xbattery_state_t *state)
{
    xassert_not_null(battery);
    xassert_not_null(state);

    if (!battery->is_init)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(battery);
    *state = battery->state;
    _unlock(battery);

    return XHAL_OK;
}

xhal_err_t xbattery_get_percentage(xbattery_t *battery, uint8_t *percentage)
{
    xassert_not_null(battery);
    xassert_not_null(percentage);

    if (!battery->is_init)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(battery);
    *percentage = battery->percentage;
    _unlock(battery);

    return XHAL_OK;
}

xhal_err_t xbattery_get_voltage(xbattery_t *battery, uint16_t *voltage_mv)
{
    xassert_not_null(battery);
    xassert_not_null(voltage_mv);

    if (!battery->is_init)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(battery);
    *voltage_mv = _get_average_voltage(battery);
    _unlock(battery);

    return XHAL_OK;
}

xhal_err_t xbattery_set_config(xbattery_t *battery,
                               const xbattery_config_t *config)
{
    xassert_not_null(battery);
    xassert_not_null(config);

    if (!battery->is_init)
    {
        return XHAL_ERR_NO_INIT;
    }

    if (config->full_voltage_mv <= config->empty_voltage_mv ||
        config->power_threshold < config->charge_threshold ||
        config->power_threshold < config->empty_voltage_mv ||
        config->charge_threshold < config->empty_voltage_mv)
    {
        return XHAL_ERR_INVALID;
    }

    _lock(battery);
    battery->config = *config;
    _calculate_percentage(battery);
    _update_battery_state(battery);
    _unlock(battery);

    return XHAL_OK;
}

xhal_err_t xbattery_get_config(xbattery_t *battery, xbattery_config_t *config)
{
    xassert_not_null(battery);
    xassert_not_null(config);

    if (!battery->is_init)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(battery);
    *config = battery->config;
    _unlock(battery);

    return XHAL_OK;
}

static void _update_battery_state(xbattery_t *battery)
{
    uint16_t avg_voltage = _get_average_voltage(battery);

    if (avg_voltage == 0)
    {
        battery->state = XBATTERY_STATE_NORMAL;
        return;
    }

    if (avg_voltage >= battery->config.power_threshold)
    {
        battery->state = XBATTERY_STATE_POWER;
    }
    else if (avg_voltage >= battery->config.charge_threshold)
    {
        battery->state = XBATTERY_STATE_CHARGING;
    }
    else if (avg_voltage >= battery->config.empty_voltage_mv)
    {
        battery->state = XBATTERY_STATE_NORMAL;
    }
    else
    {
        battery->state = XBATTERY_STATE_EMPTY;
    }
}

static void _calculate_percentage(xbattery_t *battery)
{
    uint16_t avg_voltage = _get_average_voltage(battery);

    if (avg_voltage == 0)
    {
        battery->percentage = 0;
        return;
    }

    uint16_t real_empty = battery->config.empty_voltage_mv;
    uint16_t real_full  = battery->config.full_voltage_mv;

    if (avg_voltage <= real_empty)
    {
        battery->percentage = 0;
        return;
    }

    if (avg_voltage >= real_full)
    {
        battery->percentage = 100;
        return;
    }

    /* 映射到固定表电压域 */
    const uint16_t table_empty = voltage_lookup_table[0];
    const uint16_t table_full  = voltage_lookup_table[100];

    uint16_t mapped_voltage =
        table_empty + (uint32_t)(avg_voltage - real_empty) *
                          (table_full - table_empty) / (real_full - real_empty);

    /* 二分查找最后一个 <= mapped_voltage 的索引 */
    uint8_t low  = 0;
    uint8_t high = 100;

    while (low < high)
    {
        uint8_t mid = (low + high + 1) >> 1; /* 向上取中点 */

        if (voltage_lookup_table[mid] <= mapped_voltage)
        {
            low = mid;
        }
        else
        {
            high = mid - 1;
        }
    }

    battery->percentage = low;
}

static void _update_voltage_buffer(xbattery_t *battery, uint16_t voltage_mv)
{
    /* 移动缓冲区（FIFO） */
    for (uint8_t i = (XBATTERY_VOLTAGE_BUFFER_SIZE - 1); i > 0; i--)
    {
        battery->voltage_mv[i] = battery->voltage_mv[i - 1];
    }

    /*  添加新值 */
    battery->voltage_mv[0] = voltage_mv;
}

static uint16_t _get_average_voltage(xbattery_t *battery)
{
    uint32_t sum  = 0;
    uint8_t count = 0;

    for (uint8_t i = 0; i < XBATTERY_VOLTAGE_BUFFER_SIZE; i++)
    {
        if (battery->voltage_mv[i] > 0)
        {
            sum += battery->voltage_mv[i];
            count++;
        }
    }

    if (count == 0)
    {
        return 0;
    }

    return (uint16_t)(sum / count);
}

static inline void _lock(xbattery_t *battery)
{
#ifdef XHAL_OS_SUPPORTING

    osStatus_t ret = osOK;
    ret            = osMutexAcquire(battery->mutex, osWaitForever);
    xassert(ret == osOK);
#endif
}

static inline void _unlock(xbattery_t *battery)
{
#ifdef XHAL_OS_SUPPORTING

    osStatus_t ret = osOK;
    ret            = osMutexRelease(battery->mutex);
    xassert(ret == osOK);
#endif
}

static const uint16_t voltage_lookup_table[101] = {
    /* 0% - 9% */
    3580, /* 0% */
    3592, /* 1% */
    3600, /* 2% */
    3616, /* 3% */
    3624, /* 4% */
    3628, /* 5% */
    3632, /* 6% */
    3636, /* 7% */
    3644, /* 8% */
    3648, /* 9% */

    /* 10% - 19% */
    3652, /* 10% */
    3656, /* 11% */
    3660, /* 12% */
    3664, /* 13% */
    3668, /* 14% */
    3672, /* 15% */
    3676, /* 16% */
    3680, /* 17% */
    3684, /* 18% */
    3688, /* 19% */

    /* 20% - 29% */
    3692, /* 20% */
    3691, /* 21% */
    3696, /* 22% */
    3700, /* 23% */
    3704, /* 24% */
    3708, /* 25% */
    3712, /* 26% */
    3716, /* 27% */
    3720, /* 28% */
    3724, /* 29% */

    /* 30% - 39% */
    3728, /* 30% */
    3729, /* 31% */
    3730, /* 32% */
    3731, /* 33% */
    3733, /* 34% */
    3732, /* 35% */
    3736, /* 36% */
    3740, /* 37% */
    3744, /* 38% */
    3748, /* 39% */

    /* 40% - 49% */
    3752, /* 40% */
    3756, /* 41% */
    3760, /* 42% */
    3764, /* 43% */
    3768, /* 44% */
    3772, /* 45% */
    3776, /* 46% */
    3780, /* 47% */
    3784, /* 48% */
    3788, /* 49% */

    /* 50% - 59% */
    3792, /* 50% */
    3796, /* 51% */
    3800, /* 52% */
    3804, /* 53% */
    3808, /* 54% */
    3812, /* 55% */
    3816, /* 56% */
    3824, /* 57% */
    3828, /* 58% */
    3832, /* 59% */

    /* 60% - 69% */
    3840, /* 60% */
    3848, /* 61% */
    3856, /* 62% */
    3860, /* 63% */
    3864, /* 64% */
    3876, /* 65% */
    3880, /* 66% */
    3884, /* 67% */
    3888, /* 68% */
    3892, /* 69% */

    /* 70% - 79% */
    3904, /* 70% */
    3912, /* 71% */
    3916, /* 72% */
    3924, /* 73% */
    3928, /* 74% */
    3936, /* 75% */
    3940, /* 76% */
    3948, /* 77% */
    3956, /* 78% */
    3964, /* 79% */

    /* 80% - 89% */
    3980, /* 80% */
    3984, /* 81% */
    3996, /* 82% */
    4008, /* 83% */
    4020, /* 84% */
    4032, /* 85% */
    4040, /* 86% */
    4052, /* 87% */
    4060, /* 88% */
    4072, /* 89% */

    /* 90% - 99% */
    4088, /* 90% */
    4092, /* 91% */
    4100, /* 92% */
    4108, /* 93% */
    4112, /* 94% */
    4120, /* 95% */
    4132, /* 96% */
    4140, /* 97% */
    4144, /* 98% */
    4156, /* 99% */
    4160  /* 100% */
};