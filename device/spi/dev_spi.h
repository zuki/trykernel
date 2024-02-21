#ifndef DEV_SPI_H
#define DEV_SPI_H

#include "spi_sysdep.h"

extern ER dev_spi_open(UW unit, UINT omode);
extern ER dev_spi_write(UW unit, UW sadr, void *buf, SZ size, SZ *asize);

extern void spi_set_format(UW unit, UINT data_bits, spi_cpol_t cpol, spi_cpha_t cpha, spi_order_t corder);

#endif /* DEV_SPI_H */
