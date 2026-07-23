#include "ds3231.h"
#include "xhal_assert.h"
#include "xhal_bit.h"
#include "xhal_log.h"
#include "xhal_time.h"

XLOG_TAG("DS3231");

#define REG_SEC       0x00
#define REG_MIN       0x01
#define REG_HOUR      0x02
#define REG_WDAY      0x03
#define REG_DATE      0x04
#define REG_MONTH     0x05
#define REG_YEAR      0x06

#define REG_ALM1_SEC  0x07
#define REG_ALM1_MIN  0x08
#define REG_ALM1_HOUR 0x09
#define REG_ALM1_DAY  0x0A

#define REG_ALM2_MIN  0x0B
#define REG_ALM2_HOUR 0x0C
#define REG_ALM2_DAY  0x0D

#define REG_CONTROL   0x0E
#define REG_STATUS    0x0F

#define CTRL_A1IE     0
#define CTRL_A2IE     1
#define CTRL_INTCN    2

#define STAT_A1F      0
#define STAT_A2F      1

#define DY_DT         6
#define A1M1          7
#define A1M2          7
#define A1M3          7
#define A1M4          7
#define A2M2          7
#define A2M3          7
#define A2M4          7

#define DEV_CAST(p)   ((ds3231_dev_t *)(p))

static xhal_err_t ds3231_init(void *inst);
static xhal_err_t ds3231_deinit(void *inst);
static xhal_err_t ds3231_get_time(void *inst, xhal_time_t *time,
                                  uint32_t timeout_ms);
static xhal_err_t ds3231_set_time(void *inst, const xhal_time_t *time,
                                  uint32_t timeout_ms);
static xhal_err_t ds3231_set_alarm(void *inst, const xrtc_alarm_t *alarm,
                                   uint8_t id, uint32_t timeout_ms);
static xhal_err_t ds3231_get_alarm(void *inst, xrtc_alarm_t *alarm, uint8_t id,
                                   uint32_t timeout_ms);
static xhal_err_t ds3231_clear_alarm(void *inst, uint8_t id,
                                     uint32_t timeout_ms);

const xrtc_ops_t ds3231_ops = {
    .init        = ds3231_init,
    .deinit      = ds3231_deinit,
    .set_time    = ds3231_set_time,
    .get_time    = ds3231_get_time,
    .set_alarm   = ds3231_set_alarm,
    .get_alarm   = ds3231_get_alarm,
    .clear_alarm = ds3231_clear_alarm,
};

static inline uint8_t _bcd2bin(uint8_t bcd);
static inline uint8_t _bin2bcd(uint8_t bin);
static inline xhal_err_t _set_reg(ds3231_dev_t *dev, uint8_t addr,
                                  uint32_t timeout_ms);

static xhal_err_t ds3231_init(void *inst)
{
    ds3231_dev_t *dev = DEV_CAST(inst);
    xassert_ptr_struct_not_null(dev->bus, "ds3231 bus null");

    return XHAL_OK;
}

static xhal_err_t ds3231_deinit(void *inst)
{
    return XHAL_OK;
}

static xhal_err_t ds3231_get_time(void *inst, xhal_time_t *time,
                                  uint32_t timeout_ms)
{
    xhal_err_t ret    = XHAL_OK;
    ds3231_dev_t *dev = DEV_CAST(inst);

    uint8_t buf[7] = {0};

    XHAL_TRY(ret, _set_reg(dev, REG_SEC, timeout_ms));
    XHAL_TRY(ret,
             dev->bus->read(DS3231_I2C_ADDR, buf, sizeof(buf), timeout_ms));

    time->second  = _bcd2bin(buf[0]);
    time->minute  = _bcd2bin(buf[1]);
    time->hour    = _bcd2bin(buf[2]);
    time->weekday = _bcd2bin(buf[3]);
    time->day     = _bcd2bin(buf[4]);
    time->month   = _bcd2bin(buf[5]);
    time->year    = 2000 + _bcd2bin(buf[6]);

    return ret;
}

static xhal_err_t ds3231_set_time(void *inst, const xhal_time_t *time,
                                  uint32_t timeout_ms)
{
    xhal_err_t ret    = XHAL_OK;
    ds3231_dev_t *dev = DEV_CAST(inst);

    if (!xtime_is_valid_time(time) || time->year < 2000 || time->year > 2099)
    {
        return XHAL_ERR_INVALID;
    }

    uint8_t buf[8] = {0};

    buf[0] = REG_SEC;
    buf[1] = _bin2bcd(time->second);
    buf[2] = _bin2bcd(time->minute);
    buf[3] = _bin2bcd(time->hour);
    buf[4] = _bin2bcd(time->weekday);
    buf[5] = _bin2bcd(time->day);
    buf[6] = _bin2bcd(time->month);
    buf[7] = _bin2bcd(time->year - 2000);

    XHAL_TRY(ret,
             dev->bus->write(DS3231_I2C_ADDR, buf, sizeof(buf), timeout_ms));

    return ret;
}

static xhal_err_t ds3231_set_alarm(void *inst, const xrtc_alarm_t *alarm,
                                   uint8_t id, uint32_t timeout_ms)
{
    xhal_err_t ret    = XHAL_OK;
    ds3231_dev_t *dev = DEV_CAST(inst);

    uint8_t buf[5] = {0};

    if (id > 1)
    {
        return XHAL_ERR_INVALID;
    }

    /* ---------- Alarm ---------- */
    if (id == 0)
    {
        uint8_t a1m1 = 1, a1m2 = 1, a1m3 = 1, a1m4 = 1;
        uint8_t dydt = 0;

        switch (alarm->mode)
        {
        case XRTC_ALARM_EVERY_SECOND:
            break;

        case XRTC_ALARM_MATCH_SECOND:
            a1m1 = 0;
            break;

        case XRTC_ALARM_MATCH_MIN_SEC:
            a1m1 = a1m2 = 0;
            break;

        case XRTC_ALARM_MATCH_HOUR_MIN_SEC:
            a1m1 = a1m2 = a1m3 = 0;
            break;

        case XRTC_ALARM_MATCH_DATE_TIME:
            a1m1 = a1m2 = a1m3 = a1m4 = 0;
            break;

        case XRTC_ALARM_MATCH_WEEK_TIME:
            a1m1 = a1m2 = a1m3 = a1m4 = 0;
            dydt                      = 1;
            break;

        default:
            return XHAL_ERR_INVALID;
        }

        buf[0] = REG_ALM1_SEC;
        buf[1] = (a1m1 << A1M1) | _bin2bcd(alarm->time.second);
        buf[2] = (a1m2 << A1M2) | _bin2bcd(alarm->time.minute);
        buf[3] = (a1m3 << A1M3) | _bin2bcd(alarm->time.hour);
        buf[4] = (a1m4 << A1M4) | (dydt << DY_DT) |
                 (dydt ? alarm->time.weekday : _bin2bcd(alarm->time.day));

        XHAL_TRY(ret, dev->bus->write(DS3231_I2C_ADDR, buf, 5, timeout_ms));
    }
    else
    {
        uint8_t a2m2 = 1, a2m3 = 1, a2m4 = 1;
        uint8_t dydt = 0;

        switch (alarm->mode)
        {
        case XRTC_ALARM_MATCH_MIN_SEC:
            a2m2 = 0;
            break;

        case XRTC_ALARM_MATCH_HOUR_MIN_SEC:
            a2m2 = a2m3 = 0;
            break;

        case XRTC_ALARM_MATCH_DATE_TIME:
            a2m2 = a2m3 = a2m4 = 0;
            break;

        case XRTC_ALARM_MATCH_WEEK_TIME:
            a2m2 = a2m3 = a2m4 = 0;
            dydt               = 1;
            break;

        default:
            return XHAL_ERR_INVALID;
        }

        buf[0] = REG_ALM2_MIN;
        buf[1] = (a2m2 << A2M2) | _bin2bcd(alarm->time.minute);
        buf[2] = (a2m3 << A2M3) | _bin2bcd(alarm->time.hour);
        buf[3] = (a2m4 << A2M4) | (dydt << DY_DT) |
                 (dydt ? alarm->time.weekday : _bin2bcd(alarm->time.day));

        XHAL_TRY(ret, dev->bus->write(DS3231_I2C_ADDR, buf, 4, timeout_ms));
    }

    /* ---------- Control ---------- */
    uint8_t ctrl = 0;
    XHAL_TRY(ret, _set_reg(dev, REG_CONTROL, timeout_ms));
    XHAL_TRY(ret, dev->bus->read(DS3231_I2C_ADDR, &ctrl, 1, timeout_ms));

    BIT_SET1(ctrl, CTRL_INTCN);
    BIT_SET(ctrl, (id == 0) ? CTRL_A1IE : CTRL_A2IE, alarm->state.enable);

    {
        uint8_t buf[2] = {REG_CONTROL, ctrl};
        XHAL_TRY(ret, dev->bus->write(DS3231_I2C_ADDR, buf, 2, timeout_ms));
    }

    /* 清除旧的闹钟标志，否则新闹钟不会触发 */
    XHAL_TRY(ret, ds3231_clear_alarm(inst, id, timeout_ms));

    return ret;
}

static xhal_err_t ds3231_get_alarm(void *inst, xrtc_alarm_t *alarm, uint8_t id,
                                   uint32_t timeout_ms)
{
    xhal_err_t ret    = XHAL_OK;
    ds3231_dev_t *dev = DEV_CAST(inst);

    if (id > 1)
    {
        return XHAL_ERR_INVALID;
    }

    /* ---------- Status ---------- */
    uint8_t stat = 0;
    XHAL_TRY(ret, _set_reg(dev, REG_STATUS, timeout_ms));
    XHAL_TRY(ret, dev->bus->read(DS3231_I2C_ADDR, &stat, 1, timeout_ms));

    alarm->state.fired =
        (id == ALARM1) ? BIT_GET(stat, STAT_A1F) : BIT_GET(stat, STAT_A2F);

    /* ---------- Control ---------- */
    uint8_t ctrl = 0;
    XHAL_TRY(ret, _set_reg(dev, REG_CONTROL, timeout_ms));
    XHAL_TRY(ret, dev->bus->read(DS3231_I2C_ADDR, &ctrl, 1, timeout_ms));

    alarm->state.enable =
        (id == ALARM1) ? BIT_GET(ctrl, CTRL_A1IE) : BIT_GET(ctrl, CTRL_A2IE);

    /* ---------- Alarm ---------- */
    uint8_t buf[4] = {0};
    XHAL_TRY(ret, _set_reg(dev, (id == ALARM1) ? REG_ALM1_SEC : REG_ALM2_MIN,
                           timeout_ms));
    XHAL_TRY(ret, dev->bus->read(DS3231_I2C_ADDR, buf, (id == ALARM1) ? 4 : 3,
                                 timeout_ms));

    if (id == ALARM1)
    {
        alarm->time.second = _bcd2bin(BITS_GET(buf[0], A1M1, 0));
        alarm->time.minute = _bcd2bin(BITS_GET(buf[1], A1M2, 0));
        alarm->time.hour   = _bcd2bin(BITS_GET(buf[2], A1M3 - 1, 0));

        uint8_t date = _bcd2bin(BITS_GET(buf[3], DY_DT, 0));

        if (BIT_GET(buf[3], DY_DT))
        {
            alarm->time.weekday = date;
        }
        else
        {
            alarm->time.day = date;
        }

        uint8_t a1m1 = BIT_GET(buf[0], A1M1);
        uint8_t a1m2 = BIT_GET(buf[1], A1M2);
        uint8_t a1m3 = BIT_GET(buf[2], A1M3);
        uint8_t a1m4 = BIT_GET(buf[3], A1M4);
        uint8_t dydt = BIT_GET(buf[3], DY_DT);

        if (a1m1 && a1m2 && a1m3 && a1m4)
            alarm->mode = XRTC_ALARM_EVERY_SECOND;
        else if (!a1m1 && a1m2 && a1m3 && a1m4)
            alarm->mode = XRTC_ALARM_MATCH_SECOND;
        else if (!a1m1 && !a1m2 && a1m3 && a1m4)
            alarm->mode = XRTC_ALARM_MATCH_MIN_SEC;
        else if (!a1m1 && !a1m2 && !a1m3 && a1m4)
            alarm->mode = XRTC_ALARM_MATCH_HOUR_MIN_SEC;
        else if (!a1m1 && !a1m2 && !a1m3 && !a1m4 && dydt)
            alarm->mode = XRTC_ALARM_MATCH_WEEK_TIME;
        else
            alarm->mode = XRTC_ALARM_MATCH_DATE_TIME;
    }
    else
    {
        alarm->time.minute = _bcd2bin(BITS_GET(buf[0], A2M2, 0));
        alarm->time.hour   = _bcd2bin(BITS_GET(buf[1], A2M3 - 1, 0));

        uint8_t date = _bcd2bin(BITS_GET(buf[2], DY_DT, 0));

        if (BIT_GET(buf[2], DY_DT))
        {
            alarm->time.weekday = date;
        }
        else
        {
            alarm->time.day = date;
        }

        uint8_t a2m2 = BIT_GET(buf[0], A2M2);
        uint8_t a2m3 = BIT_GET(buf[1], A2M3);
        uint8_t a2m4 = BIT_GET(buf[2], A2M4);
        uint8_t dydt = BIT_GET(buf[2], DY_DT);

        if (a2m2 && a2m3 && a2m4)
            alarm->mode = XRTC_ALARM_EVERY_SECOND;
        else if (!a2m2 && a2m3 && a2m4)
            alarm->mode = XRTC_ALARM_MATCH_MIN_SEC;
        else if (!a2m2 && !a2m3 && a2m4)
            alarm->mode = XRTC_ALARM_MATCH_HOUR_MIN_SEC;
        else if (!a2m2 && !a2m3 && !a2m4 && dydt)
            alarm->mode = XRTC_ALARM_MATCH_WEEK_TIME;
        else
            alarm->mode = XRTC_ALARM_MATCH_DATE_TIME;
    }

    return ret;
}

static xhal_err_t ds3231_clear_alarm(void *inst, uint8_t id,
                                     uint32_t timeout_ms)
{
    xhal_err_t ret    = XHAL_OK;
    ds3231_dev_t *dev = DEV_CAST(inst);

    uint8_t stat = 0;
    XHAL_TRY(ret, _set_reg(dev, REG_STATUS, timeout_ms));
    XHAL_TRY(ret, dev->bus->read(DS3231_I2C_ADDR, &stat, 1, timeout_ms));

    BIT_SET0(stat, (id == 0) ? STAT_A1F : STAT_A2F);

    {
        uint8_t buf[2] = {REG_STATUS, stat};
        XHAL_TRY(ret, dev->bus->write(DS3231_I2C_ADDR, buf, 2, timeout_ms));
    }

    return ret;
}

static inline uint8_t _bcd2bin(uint8_t bcd)
{
    return (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0F));
}

static inline uint8_t _bin2bcd(uint8_t bin)
{
    return (uint8_t)(((bin / 10) << 4) | (bin % 10));
}

static inline xhal_err_t _set_reg(ds3231_dev_t *dev, uint8_t addr,
                                  uint32_t timeout_ms)
{
    return dev->bus->write(DS3231_I2C_ADDR, &addr, 1, timeout_ms);
}
