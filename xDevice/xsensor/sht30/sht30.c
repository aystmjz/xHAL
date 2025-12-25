#include "sht30.h"
#include "xhal_assert.h"
#include "xhal_bit.h"
#include "xhal_coro.h"
#include "xhal_crc.h"
#include "xhal_log.h"
#include "xhal_time.h"

XLOG_TAG("SHT30");

#define SHT30_CMD_MEAS_HIGHREP 0x2400 /* 测量命令: 无时钟拉伸，高重复性 */
#define SHT30_CMD_MEAS_MEDREP  0x240B /* 测量命令: 无时钟拉伸，中等重复性 */
#define SHT30_CMD_MEAS_LOWREP  0x2416 /* 测量命令: 无时钟拉伸，低重复性 */
#define SHT30_CMD_SOFT_RESET   0X30A2 /* 软件复位命令 */

#define SHT30_DEV_CAST(_inst)  ((sht30_dev_t *)(_inst))

static xhal_err_t sht30_init(void *inst);
static xhal_err_t sht30_deinit(void *inst);
static void sht30_reset(xcoro_handle_t *handle, void *inst,
                        xsensor_event_t *event);
static void sht30_read(xcoro_handle_t *handle, void *inst,
                       xsensor_event_t *event);

const xsensor_ops_t sht30_ops = {
    .init   = sht30_init,
    .deinit = sht30_deinit,
    .reset  = sht30_reset,
    .read   = sht30_read,
};

static xhal_err_t _sht30_write_cmd(sht30_dev_t *dev, uint16_t cmd,
                                   uint32_t timeout_ms);

static xhal_err_t sht30_init(void *inst)
{
    sht30_dev_t *dev = SHT30_DEV_CAST(inst);
    xassert_ptr_struct_not_null(dev->bus, "sht30_bus_ops is null");

    return XHAL_OK;
}

static xhal_err_t sht30_deinit(void *inst)
{
    return XHAL_OK;
}

static void sht30_reset(xcoro_handle_t *handle, void *inst,
                        xsensor_event_t *event)
{
    sht30_dev_t *dev = SHT30_DEV_CAST(inst);
    xhal_err_t ret   = XHAL_OK;

    XCORO_BEGIN(handle);
    ret = _sht30_write_cmd(dev, SHT30_CMD_SOFT_RESET, event->timeout_ms);

    if (ret != XHAL_OK)
    {
        XHAL_GOTO_IF_ERROR(ret, dev->reset->trigger(true), exit);
        XCORO_DELAY_MS(handle, 20);
        XHAL_GOTO_IF_ERROR(ret, dev->reset->trigger(false), exit);
        XCORO_DELAY_MS(handle, 20);
    }

exit:
    if (event->cb)
    {
        event->cb(event, NULL, ret);
    }
    XCORO_END(handle);
}

static void sht30_read(xcoro_handle_t *handle, void *inst,
                       xsensor_event_t *event)
{
    sht30_dev_t *dev = SHT30_DEV_CAST(inst);
    xhal_err_t ret   = XHAL_OK;

    uint16_t t_raw = 0, h_raw = 0;
    sht30_data_t data;

    XCORO_BEGIN(handle);

    XHAL_GOTO_IF_ERROR(
        ret, _sht30_write_cmd(dev, SHT30_CMD_MEAS_HIGHREP, event->timeout_ms),
        exit);

    XCORO_DELAY_MS(handle, SHT30_MEAS_DELAY_MS);

    XHAL_GOTO_IF_ERROR(
        ret,
        dev->bus->read(SHT30_I2C_ADDR, data.raw_data, 6, event->timeout_ms),
        exit);

    if (xcrc8(XCRC8_INIT, &data.raw_data[0], 2) != data.raw_data[2] ||
        xcrc8(XCRC8_INIT, &data.raw_data[3], 2) != data.raw_data[5])
    {
        ret = XHAL_ERR_CRC;
        goto exit;
    }

    BITS_SET(t_raw, 8, 0, data.raw_data[1]);
    BITS_SET(t_raw, 8, 8, data.raw_data[0]);
    BITS_SET(h_raw, 8, 0, data.raw_data[4]);
    BITS_SET(h_raw, 8, 8, data.raw_data[3]);

    data.temp = -45.0f + 175.0f * ((float)t_raw / 65535.0f);
    data.hum  = 100.0f * ((float)h_raw / 65535.0f);
    data.hum  = data.hum > 100.0f ? 100.0f : data.hum;
    data.hum  = data.hum < 0.0f ? 0.0f : data.hum;

exit:
    if (event->cb)
    {
        event->cb(event, &data, ret);
    }
    XCORO_END(handle);
}

static xhal_err_t _sht30_write_cmd(sht30_dev_t *dev, uint16_t cmd,
                                   uint32_t timeout_ms)
{
    uint8_t buff[2] = {
        BITS_GET(cmd, 8, 8),
        BITS_GET(cmd, 8, 0),
    };
    return dev->bus->write(SHT30_I2C_ADDR, buff, 2, timeout_ms);
}
