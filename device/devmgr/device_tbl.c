#include <trykernel.h>
#include "device.h"
#include "../i2c/dev_i2c.h"
#include "../spi/dev_spi.h"

/* デバイス登録テーブル*/
T_DEV_DEF   dev_tbl[DEV_NUM] = {
    {"iica", 0, 0, 0, dev_i2c_open, dev_i2c_read, dev_i2c_write},   // I2C0
    {"spia", 0, 0, 0, dev_spi_open, 0, dev_spi_write},  // SPI0
};
