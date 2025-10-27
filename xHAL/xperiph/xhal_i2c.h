#ifndef __XHAL_I2C_H
#define __XHAL_I2C_H

#include "xhal_periph.h"

#define XI2C_EVENT_DONE (1U << 0)

#define XI2C_WD         0x0000 /* 写操作 */
#define XI2C_RD         0x0001 /* 读操作 */
#define XI2C_TEN        0x0010 /* 使用 10 位地址 */
#define XI2C_RECV_LEN   0x0400 /* 读数据长度在第一字节返回 */
#define XI2C_IGNORE_NAK 0x1000 /* 忽略 NACK 信号 */
#define XI2C_NOSTART    0x4000 /* 不发送重复 START */
#define XI2C_STOP       0x8000 /* 发送 STOP 信号 */

#define XI2C_CONFIG_DEFAULT                         \
    {                                               \
        100000, /* Default 100 kHz standard mode */ \
    }

typedef struct xhal_i2c xhal_i2c_t;

typedef struct xhal_i2c_msg
{
    uint16_t addr;
    uint16_t flags;
    uint16_t len;
    uint8_t *buf;
} xhal_i2c_msg_t;

typedef struct xhal_i2c_config
{
    uint32_t clock;
} xhal_i2c_config_t;

typedef struct xhal_i2c_ops
{
    xhal_err_t (*init)(xhal_i2c_t *self);
    xhal_err_t (*config)(xhal_i2c_t *self, const xhal_i2c_config_t *config);
    xhal_err_t (*transfer)(xhal_i2c_t *self, xhal_i2c_msg_t *msg);
} xhal_i2c_ops_t;

typedef struct xhal_i2c_data
{
    xhal_i2c_config_t config;

#ifdef XHAL_OS_SUPPORTING
    osEventFlagsId_t event_flag;
#endif
    const char *i2c_name;
    const char *sda_name;
    const char *scl_name;
} xhal_i2c_data_t;

typedef struct xhal_i2c
{
    xhal_periph_t peri;

    const xhal_i2c_ops_t *ops;
    xhal_i2c_data_t data;
} xhal_i2c_t;

#define XHAL_I2C_CAST(_dev) ((xhal_i2c_t *)_dev)

xhal_err_t xi2c_inst(xhal_i2c_t *self, const char *name,
                     const xhal_i2c_ops_t *ops, const char *i2c_name,
                     const char *sda_name, const char *scl_name,
                     const xhal_i2c_config_t *config);

xhal_err_t xi2c_transfer(xhal_periph_t *self, xhal_i2c_msg_t *msgs,
                         uint32_t num, uint32_t timeout_ms);
xhal_err_t xi2c_read(xhal_periph_t *self, uint16_t addr, uint8_t *buff,
                     uint16_t size, uint16_t flags, uint32_t timeout_ms);
xhal_err_t xi2c_write(xhal_periph_t *self, uint16_t addr, uint8_t *buff,
                      uint16_t len, uint16_t flags, uint32_t timeout_ms);
xhal_err_t xi2c_write_read(xhal_periph_t *self, uint16_t addr, uint8_t *wbuf,
                           uint16_t wlen, uint8_t *rbuf, uint16_t rlen,
                           uint16_t flags, uint32_t timeout_ms);

xhal_err_t xi2c_set_config(xhal_periph_t *self, xhal_i2c_config_t *config);
xhal_err_t xi2c_get_config(xhal_periph_t *self, xhal_i2c_config_t *config);

#endif /* __XHAL_I2C_H */