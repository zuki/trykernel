#ifndef    DEV_SPI_RP2040_H
#define    DEV_SPI_RP2040_H

#define    DEV_SPI_UNITNM      (2)    /* Number of devive units */
#define    DEV_SPI_0           (0)    /* SPI1 */
#define    DEV_SPI_1           (1)    /* SPI2 */


// SPIのGPIOピン番号
//  SPI0
#define SPI0_RX     16
#define SPI0_CS     17
#define SPI0_SCK    18
#define SPI0_TX     19
// SPI1
#define SPI1_RX     12
#define SPI1_CS     13
#define SPI1_SCK    14
#define SPI1_TX     15


/*
 * SPI CPHA (クロックの位相) 値の列挙型.
 */
typedef enum {
    SPI_CPHA_0 = 0, /*!< 0/1でデータ取り込み */
    SPI_CPHA_1 = 1  /*!< 1/0でデータ取り込み */
} spi_cpha_t;

/*
 * SPI CPOL (クロックの極性) 値の列挙型.
 */
typedef enum {
    SPI_CPOL_0 = 0, /*!< 正論理 */
    SPI_CPOL_1 = 1  /*!< 負論理 */
} spi_cpol_t;

/*
 * SPI ビット順値の列挙型.
 */
typedef enum {
    SPI_LSB_FIRST = 0,  /*!< LSBから */
    SPI_MSB_FIRST = 1   /*!< MSBから */
} spi_order_t;

/*----------------------------------------------------------------------
 * SPI レジスタ
 * --------------------------------------------------------------------*/

/* Base address */
#define    SPI0_BASE               0x4003c000
#define    SPI1_BASE               0x40040000

/* Register offset */
#define SPIx_CR0         (0x000)   // Control register 0
#define SPIx_CR1         (0x004)   // Control register 1
#define SPIx_DR          (0x008)   // Data register
#define SPIx_SR          (0x00c)   // Status register
#define SPIx_CPSR        (0x010)   // Clock prescale register
#define SPIx_IMSC        (0x014)   // Interrupt mask set or clear register
#define SPIx_RIS         (0x018)   // Raw interrupt status register
#define SPIx_MIS         (0x01c)   // Masked interrupt status register
#define SPIx_ICR         (0x020)   // Interrupt clear register
#define SPIx_DMACR       (0x024)   // DMA control register
#define SPIx_PERIPHID0   (0xfe0)   // Peripheral identification 0 registers
#define SPIx_PERIPHID1   (0xfe4)   // Peripheral identification 1 registers
#define SPIx_PERIPHID2   (0xfe8)   // Peripheral identification 2 registers
#define SPIx_PERIPHID3   (0xfec)   // Peripheral identification 3 registers
#define SPIx_PCELLID0    (0xff0)   // PrimeCell identification 0 registers
#define SPIx_PCELLID1    (0xff4)   // PrimeCell identification 1 registers
#define SPIx_PCELLID2    (0xff8)   // PrimeCell identification 2 registers
#define SPIx_PCELLID3    (0xffc)   // PrimeCell identification 3 registers

/* Register bit definition */
#define SPI_CR0_SCR      (0xff < 8)  // Serial clock rate.
#define SPI_CR0_SPH      (1 < 7)     // CLKOUT phase
#define SPI_CR0_SPO      (1 < 6)     // CLKOUT polarity
#define SPI_CR0_FRF      (3 < 4)     // Frame format
#define SPI_CR0_DSS      (0xf)       // Data Size

#define SPI_CR1_SOD      (1 << 3)     // Slave-mode output disable
#define SPI_CR1_MS       (1 << 2)     // Master or slave mode select
#define SPI_CR1_SSE      (1 << 1)     // Synchronous serial port enable
#define SPI_CR1_LBM      (1 << 0)     // Loop back mode

#define SPI_DR_DATA      (0xffff)    // Transmit/Receive FIFO

#define SPI_SR_BSY       (1 << 4)     // PrimeCell SSP busy flag
#define SPI_SR_RFF       (1 << 3)     // Receive FIFO full
#define SPI_SR_RNE       (1 << 2)     // Receive FIFO not empty
#define SPI_SR_TNF       (1 << 1)     // Transmit FIFO not full
#define SPI_SR_TFE       (1 << 0)     // Transmit FIFO empty

#define SPI_CPSR_CPSDVSR (0xff)      // Clock prescale divisor

#define SPI_IMSC_TXIM    (1 << 3)     // Transmit FIFO interrupt mask
#define SPI_IMSC_RXIM    (1 << 2)     // Receive FIFO interrupt mask
#define SPI_IMSC_RTIM    (1 << 1)     // Receive timeout interrupt mask
#define SPI_IMSC_RORIM   (1 << 0)     // Receive overrun interrupt mask

#define SPI_RIS_TXRIS    (1 << 3)     // raw interrupt state of TXINTR
#define SPI_RIS_RXRIS    (1 << 2)     // raw interrupt state of RXINTR
#define SPI_RIS_RTRIS    (1 << 1)     // raw interrupt state of RTINTR
#define SPI_RIS_RORRIS   (1 << 0)     // raw interrupt state of RORINTR

#define SPI_MIS_TXMIS    (1 << 3)     // masked interrupt state of TXINTR
#define SPI_MIS_RXMIS    (1 << 2)     // masked interrupt state of RXINTR
#define SPI_MIS_RTMIS    (1 << 1)     // masked interrupt state of RTINTR
#define SPI_MIS_RORMIS   (1 << 0)     // masked interrupt state of RORINTR

#define SPI_ICR_RTIC     (1 << 1)     // Clears the RTINTR interrupt
#define SPI_ICR_RORIC    (1 << 0)    // Clears the RORINTR interrupt

#define SPI_DMACR_TXDMAE (1 << 1)     // Transmit DMA Enable
#define SPI_DMACR_RXDMAE (1 << 0)     // Receive DMA Enable


/*----------------------------------------------------------------------
 * SPI レジスタアクセス・マクロ
 *---------------------------------------------------------------------*/
#define SPI_CR0(u)	        (ba[u] + SPIx_CR0)	//  Control register 0
#define SPI_CR1(u)	        (ba[u] + SPIx_CR1)	//  Control register 1
#define SPI_DR(u)	        (ba[u] + SPIx_DR)	//  Data register
#define SPI_SR(u)	        (ba[u] + SPIx_SR)	//  Status register
#define SPI_CPSR(u)	        (ba[u] + SPIx_CPSR)	//  Clock prescale register
#define SPI_IMSC(u)	        (ba[u] + SPIx_IMSC)	//  Interrupt mask set or clear register
#define SPI_RIS(u)	        (ba[u] + SPIx_RIS)	//  Raw interrupt status register
#define SPI_MIS(u)	        (ba[u] + SPIx_MIS)	//  Masked interrupt status register
#define SPI_ICR(u)	        (ba[u] + SPIx_ICR)	//  Interrupt clear register
#define SPI_DMACR(u)	    (ba[u] + SPIx_DMACR)    //  DMA control register
#define SPI_PERIPHID0(u)    (ba[u] + SPIx_PERIPHID0)    //  Peripheral identification 0 registers
#define SPI_PERIPHID1(u)	(ba[u] + SPIx_PERIPHID1)	//  Peripheral identification 1 registers
#define SPI_PERIPHID2(u)	(ba[u] + SPIx_PERIPHID2)	//  Peripheral identification 2 registers
#define SPI_PERIPHID3(u)    (ba[u] + SPIx_PERIPHID3)	//  Peripheral identification 3 registers
#define SPI_PCELLID0(u)	    (ba[u] + SPIx_PCELLID0)	    //  PrimeCell identification 0 registers
#define SPI_PCELLID1(u)	    (ba[u] + SPIx_PCELLID1)	    //  PrimeCell identification 1 registers
#define SPI_PCELLID2(u)	    (ba[u] + SPIx_PCELLID2)	    //  PrimeCell identification 2 registers
#define SPI_PCELLID3(u)	    (ba[u] + SPIx_PCELLID3)	    //  PrimeCell identification 3 registers

#endif      /* DEV_SPI_RP2040_H */
