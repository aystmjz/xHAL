#ifndef __SHT30_H
#define __SHT30_H

#include "../xsensor.h"
#include "xhal_def.h"

#define SHT30_I2C_ADDR      0x44 /* SHT30 I2C地址 */
#define SHT30_MEAS_DELAY_MS 40   /* SHT30 测量延时 */

typedef struct sht30_data
{
    float temp;          /* 温度值(摄氏度) */
    float hum;           /* 湿度值(相对湿度%) */
    uint8_t raw_data[6]; /* 原始数据缓冲区(温度2字节+CRC+湿度2字节+CRC) */
} sht30_data_t;

#define SHT30_DATA_CAST(_data) ((sht30_data_t *)(_data))

typedef struct sht30_bus_ops
{
    xhal_err_t (*read)(uint16_t addr, uint8_t *buff, uint16_t size,
                       uint32_t timeout_ms);
    xhal_err_t (*write)(uint16_t addr, uint8_t *buff, uint16_t len,
                        uint32_t timeout_ms);
} sht30_bus_ops_t;

typedef struct sht30_reset_ops
{
    xhal_err_t (*trigger)(bool state);
} sht30_reset_ops_t;

typedef struct sht30_dev
{
    const sht30_bus_ops_t *bus;
    const sht30_reset_ops_t *reset;
} sht30_dev_t;

extern const xsensor_ops_t sht30_ops;

#endif /* __SHT30_H */