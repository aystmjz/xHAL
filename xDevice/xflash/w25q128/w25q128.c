#include "w25q128.h"
#include "xhal_assert.h"
#include "xhal_bit.h"
#include "xhal_coro.h"
#include "xhal_log.h"
#include "xhal_time.h"

XLOG_TAG("W25Q128");

#define W25Q128_PAGE_SIZE              256U
#define W25Q128_SECTOR_SIZE            (4U * 1024U)
#define W25Q128_FLASH_SIZE_BYTES       (16UL * 1024 * 1024)

#define W25Q128_WRITE_ENABLE           0x06 /* 写使能指令 */
#define W25Q128_READ_STATUS_REGISTER_1 0x05 /* 读状态寄存器1指令 */
#define W25Q128_PAGE_PROGRAM           0x02 /* 页编程指令 */
#define W25Q128_SECTOR_ERASE_4KB       0x20 /* 4KB扇区擦除指令 */
#define W25Q128_BLOCK_ERASE_64KB       0xD8 /* 64KB块擦除指令 */
#define W25Q128_CHIP_ERASE             0xC7 /* 芯片全擦除指令 */
#define W25Q128_READ_DATA              0x03 /* 读数据指令 */
#define W25Q128_JEDEC_ID               0x9F /* JEDEC ID指令 */

#define W25Q128_DEV_CAST(_dev)         ((w25q128_dev_t *)_dev)

static xhal_err_t w25q128_init(void *inst);
static xhal_err_t w25q128_deinit(void *inst);
static xhal_err_t w25q128_read_id(void *inst, uint8_t *manufacturer_id,
                                  uint16_t *device_id, uint32_t timeout_ms);
static xhal_err_t w25q128_read(void *inst, uint32_t address, uint8_t *buffer,
                               uint32_t size, uint32_t timeout_ms);
static xhal_err_t w25q128_write(void *inst, uint32_t address,
                                const uint8_t *data, uint32_t size,
                                uint32_t timeout_ms);
static void w25q128_erase(xcoro_handle_t *handle, void *inst,
                          xflash_event_t *event);

const xflash_ops_t w25q128_ops = {
    .init    = w25q128_init,
    .deinit  = w25q128_deinit,
    .read_id = w25q128_read_id,
    .read    = w25q128_read,
    .write   = w25q128_write,
    .erase   = w25q128_erase,
};

static xhal_err_t _write_enable(w25q128_dev_t *dev, uint32_t timeout_ms);
static xhal_err_t _wait_busy(w25q128_dev_t *dev, uint32_t timeout_ms);

static xhal_err_t w25q128_init(void *inst)
{
    w25q128_dev_t *dev = W25Q128_DEV_CAST(inst);
    xassert_ptr_struct_not_null(dev->bus, "w25q128_bus_ops is null");

    return XHAL_OK;
}

static xhal_err_t w25q128_deinit(void *inst)
{
    return XHAL_OK;
}

static xhal_err_t w25q128_read_id(void *inst, uint8_t *manufacturer_id,
                                  uint16_t *device_id, uint32_t timeout_ms)
{
    w25q128_dev_t *dev = W25Q128_DEV_CAST(inst);
    uint8_t cmd        = W25Q128_JEDEC_ID;
    uint8_t recv[3]    = {0};
    xhal_err_t ret     = XHAL_OK;

    XHAL_GOTO_IF_ERROR(ret, _wait_busy(dev, timeout_ms), exit);

    XHAL_GOTO_IF_ERROR(ret, dev->bus->cs_select(), exit);
    XHAL_GOTO_IF_ERROR(ret, dev->bus->transfer(&cmd, NULL, 1, timeout_ms),
                       deselect);
    XHAL_GOTO_IF_ERROR(ret, dev->bus->transfer(NULL, recv, 3, timeout_ms),
                       deselect);

    *manufacturer_id = recv[0];
    BITS_SET(*device_id, 8, 8, recv[1]);
    BITS_SET(*device_id, 8, 0, recv[2]);

deselect:
    dev->bus->cs_deselect();
exit:
    return ret;
}

static xhal_err_t w25q128_read(void *inst, uint32_t address, uint8_t *buffer,
                               uint32_t size, uint32_t timeout_ms)
{
    w25q128_dev_t *dev = W25Q128_DEV_CAST(inst);
    xhal_err_t ret     = XHAL_OK;

    if ((address >= W25Q128_FLASH_SIZE_BYTES) ||
        (size > W25Q128_FLASH_SIZE_BYTES - address))
    {
        return XHAL_ERR_INVALID;
    }

    uint8_t cmd[4] = {
        W25Q128_READ_DATA,
        BITS_GET(address, 8, 16),
        BITS_GET(address, 8, 8),
        BITS_GET(address, 8, 0),
    };

    XHAL_GOTO_IF_ERROR(ret, _wait_busy(dev, timeout_ms), exit);

    XHAL_GOTO_IF_ERROR(ret, dev->bus->cs_select(), exit);
    XHAL_GOTO_IF_ERROR(ret, dev->bus->transfer(cmd, NULL, 4, timeout_ms),
                       deselect);
    XHAL_GOTO_IF_ERROR(ret, dev->bus->transfer(NULL, buffer, size, timeout_ms),
                       deselect);
deselect:
    dev->bus->cs_deselect();
exit:
    return ret;
}

static xhal_err_t w25q128_write(void *inst, uint32_t address,
                                const uint8_t *data, uint32_t size,
                                uint32_t timeout_ms)
{
    w25q128_dev_t *dev = W25Q128_DEV_CAST(inst);
    xhal_err_t ret     = XHAL_OK;

    if ((address >= W25Q128_FLASH_SIZE_BYTES) ||
        (size > W25Q128_FLASH_SIZE_BYTES - address))
    {
        return XHAL_ERR_INVALID;
    }

    XHAL_GOTO_IF_ERROR(ret, _wait_busy(dev, timeout_ms), exit);
    XHAL_GOTO_IF_ERROR(ret, _write_enable(dev, timeout_ms), exit);

    XHAL_GOTO_IF_ERROR(ret, dev->bus->cs_select(), exit);
    while (size)
    {
        uint32_t page_off  = address % W25Q128_PAGE_SIZE;
        uint32_t write_len = W25Q128_PAGE_SIZE - page_off;

        if (write_len > size)
        {
            write_len = size;
        }

        uint8_t cmd[4] = {
            W25Q128_PAGE_PROGRAM,
            BITS_GET(address, 8, 16),
            BITS_GET(address, 8, 8),
            BITS_GET(address, 8, 0),
        };
        XHAL_GOTO_IF_ERROR(ret, dev->bus->transfer(cmd, NULL, 4, timeout_ms),
                           deselect);
        XHAL_GOTO_IF_ERROR(
            ret, dev->bus->transfer(data, NULL, write_len, timeout_ms),
            deselect);

        address += write_len;
        data += write_len;
        size -= write_len;
    }
deselect:
    dev->bus->cs_deselect();
exit:
    return ret;
}

static void w25q128_erase(xcoro_handle_t *handle, void *inst,
                          xflash_event_t *event)
{
    static xhal_tick_t start_tick = 0;

    w25q128_dev_t *dev = W25Q128_DEV_CAST(inst);
    xhal_err_t ret     = XHAL_OK;

    uint8_t cmd[4] = {
        W25Q128_SECTOR_ERASE_4KB,
        BITS_GET(event->address, 8, 16),
        BITS_GET(event->address, 8, 8),
        BITS_GET(event->address, 8, 0),
    };

    switch (event->type)
    {
    case XFLASH_ERASE_SECTOR:
        cmd[0] = W25Q128_SECTOR_ERASE_4KB;
        break;
    case XFLASH_ERASE_BLOCK:
        cmd[0] = W25Q128_BLOCK_ERASE_64KB;
        break;
    case XFLASH_ERASE_CHIP:
        cmd[0] = W25Q128_CHIP_ERASE;
        break;
    default:
        ret = XHAL_ERR_NOT_SUPPORT;
        goto exit;
    }

    XCORO_BEGIN(handle);
    if (event->address > W25Q128_FLASH_SIZE_BYTES)
    {
        ret = XHAL_ERR_INVALID;
        goto exit;
    }

    XHAL_GOTO_IF_ERROR(ret, _wait_busy(dev, event->timeout_ms), exit);
    XHAL_GOTO_IF_ERROR(ret, _write_enable(dev, event->timeout_ms), exit);

    XHAL_GOTO_IF_ERROR(ret, dev->bus->cs_select(), exit);
    XHAL_GOTO_IF_ERROR(ret, dev->bus->transfer(cmd, NULL, 4, event->timeout_ms),
                       deselect);

    start_tick = xtime_get_tick_ms();
    while (_wait_busy(dev, 0) != XHAL_OK)
    {
        if (TIME_DIFF(xtime_get_tick_ms(), start_tick) >= event->timeout_ms)
        {
            ret = XHAL_ERR_TIMEOUT;
            break;
        }

        XCORO_DELAY_MS(handle, 1);
    }

deselect:
    dev->bus->cs_deselect();
exit:
    if (event->cb)
    {
        event->cb(event, ret);
    }
    XCORO_END(handle);
}

static xhal_err_t _write_enable(w25q128_dev_t *dev, uint32_t timeout_ms)
{
    uint8_t cmd    = W25Q128_WRITE_ENABLE;
    xhal_err_t ret = XHAL_OK;

    XHAL_GOTO_IF_ERROR(ret, dev->bus->cs_select(), exit);
    XHAL_GOTO_IF_ERROR(ret, dev->bus->transfer(&cmd, NULL, 1, timeout_ms),
                       deselect);

deselect:
    dev->bus->cs_deselect();
exit:
    return ret;
}

static xhal_err_t _wait_busy(w25q128_dev_t *dev, uint32_t timeout_ms)
{
    uint8_t cmd    = W25Q128_READ_STATUS_REGISTER_1;
    uint8_t status = 0;
    xhal_err_t ret = XHAL_OK;

    timeout_ms++;
    while (timeout_ms--)
    {
        XHAL_GOTO_IF_ERROR(ret, dev->bus->cs_select(), exit);
        XHAL_GOTO_IF_ERROR(ret, dev->bus->transfer(&cmd, NULL, 1, 10),
                           deselect);
        XHAL_GOTO_IF_ERROR(ret, dev->bus->transfer(NULL, &status, 1, 10),
                           deselect);
        dev->bus->cs_deselect();

        /* WIP = bit0, 0 表示空闲 */
        if (BIT_GET(status, 0) == 0)
        {
            return XHAL_OK;
        }

        if (timeout_ms)
        {
            xtime_delay_ms(1);
        }
    }

    return XHAL_ERR_TIMEOUT;
deselect:
    dev->bus->cs_deselect();
exit:
    return ret;
}
