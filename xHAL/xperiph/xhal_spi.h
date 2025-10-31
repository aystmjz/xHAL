#ifndef __XHAL_SPI_H
#define __XHAL_SPI_H

#include "xhal_periph.h"

#define XSPI_EVENT_TX_DONE (1U << 0)
#define XSPI_EVENT_RX_DONE (1U << 1)
#define XSPI_EVENT_DONE    (XSPI_EVENT_TX_DONE | XSPI_EVENT_RX_DONE)

#define XSPI_CONFIG_DEFAULT         \
    {                               \
        XSPI_MODE_0,                \
        XSPI_DIR_2LINE_FULL_DUPLEX, \
        XSPI_DATA_BITS_8,           \
    }

enum xhal_spi_mode
{
    XSPI_MODE_0 = 0, /* CPOL = 0, CPHA = 0 */
    XSPI_MODE_1,     /* CPOL = 0, CPHA = 1 */
    XSPI_MODE_2,     /* CPOL = 1, CPHA = 0 */
    XSPI_MODE_3      /* CPOL = 1, CPHA = 1 */
};

enum xhal_spi_direction
{
    XSPI_DIR_2LINE_FULL_DUPLEX = 0, /* 双线全双工 */
    XSPI_DIR_2LINE_RX_ONLY,         /* 双线只接收 */
    XSPI_DIR_1LINE_RX,              /* 单线接收 */
    XSPI_DIR_1LINE_TX               /* 单线发送 */
};

enum xhal_spi_bits
{
    XSPI_DATA_BITS_8 = 0,
    XSPI_DATA_BITS_16
};

typedef struct xhal_spi xhal_spi_t;

typedef struct xhal_spi_msg
{
    const void *tx_buf;
    void *rx_buf;
    uint32_t len;
} xhal_spi_msg_t;

typedef struct xhal_spi_config
{
    uint8_t mode;
    uint8_t direction;
    uint8_t data_bits;
} xhal_spi_config_t;

typedef struct xhal_spi_ops
{
    xhal_err_t (*init)(xhal_spi_t *self);
    xhal_err_t (*config)(xhal_spi_t *self, const xhal_spi_config_t *config);
    xhal_err_t (*transfer)(xhal_spi_t *self, xhal_spi_msg_t *msg);
} xhal_spi_ops_t;

typedef struct xhal_spi_data
{
    xhal_spi_config_t config;

#ifdef XHAL_OS_SUPPORTING
    osEventFlagsId_t event_flag;
#endif
    const char *spi_name;

    const char *sck_name;
    const char *miso_name;
    const char *mosi_name;
} xhal_spi_data_t;

typedef struct xhal_spi
{
    xhal_periph_t peri;

    const xhal_spi_ops_t *ops;
    xhal_spi_data_t data;
} xhal_spi_t;

#define XSPI_CAST(_dev) ((xhal_spi_t *)_dev)

xhal_err_t xspi_inst(xhal_spi_t *self, const char *name,
                     const xhal_spi_ops_t *ops, const char *spi_name,
                     const char *sck_name, const char *mosi_name,
                     const char *miso_name, const xhal_spi_config_t *config);

xhal_err_t xspi_transfer(xhal_periph_t *self, xhal_spi_msg_t *msgs,
                         uint32_t num, uint32_t timeout_ms);
xhal_err_t xspi_read(xhal_periph_t *self, uint8_t *buff, uint16_t len,
                     uint32_t timeout_ms);
xhal_err_t xspi_write(xhal_periph_t *self, uint8_t *buff, uint16_t len,
                      uint32_t timeout_ms);
xhal_err_t xspi_write_read(xhal_periph_t *self, uint8_t *wbuf, uint16_t wlen,
                           uint8_t *rbuf, uint16_t rlen, uint32_t timeout_ms);

xhal_err_t xspi_set_config(xhal_periph_t *self, xhal_spi_config_t *config);
xhal_err_t xspi_get_config(xhal_periph_t *self, xhal_spi_config_t *config);
xhal_err_t xspi_set_direction(xhal_periph_t *self, uint8_t direction);

#endif /* __XHAL_SPI_H */