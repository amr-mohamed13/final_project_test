/**
 * Spi_Private.h — SPI1 register definitions for STM32F401
 */

#ifndef SPI_PRIVATE_H
#define SPI_PRIVATE_H

#include "Std_Types.h"

/* SPI register map */
typedef struct {
    volatile uint32 CR1;         /* 0x00 - Control register 1    */
    volatile uint32 CR2;         /* 0x04 - Control register 2    */
    volatile uint32 SR;          /* 0x08 - Status register       */
    volatile uint32 DR;          /* 0x0C - Data register         */
    volatile uint32 CRCPR;       /* 0x10 - CRC polynomial        */
    volatile uint32 RXCRCR;      /* 0x14 - RX CRC                */
    volatile uint32 TXCRCR;      /* 0x18 - TX CRC                */
    volatile uint32 I2SCFGR;     /* 0x1C - I2S configuration     */
    volatile uint32 I2SPR;       /* 0x20 - I2S prescaler         */
} SpiType;

#define SPI1_BASE_ADDR      0x40013000UL
#define SPI1                 ((SpiType *)SPI1_BASE_ADDR)

/* CR1 bit positions */
#define SPI_CR1_CPHA         0U
#define SPI_CR1_CPOL         1U
#define SPI_CR1_MSTR         2U
#define SPI_CR1_BR0          3U     /* Baud rate [5:3] */
#define SPI_CR1_SPE          6U     /* SPI enable */
#define SPI_CR1_SSI          8U     /* Internal slave select */
#define SPI_CR1_SSM          9U     /* Software slave management */
#define SPI_CR1_DFF          11U    /* Data frame format: 0=8bit, 1=16bit */

/* CR2 bit positions */
#define SPI_CR2_RXNEIE       6U     /* RX not empty interrupt enable */
#define SPI_CR2_TXEIE        7U     /* TX empty interrupt enable */
#define SPI_CR2_SSOE         2U     /* SS output enable */

/* SR bit positions */
#define SPI_SR_RXNE          0U     /* Receive buffer not empty */
#define SPI_SR_TXE           1U     /* Transmit buffer empty */
#define SPI_SR_BSY           7U     /* Busy flag */
#define SPI_SR_OVR           6U     /* Overrun flag */

/* SPI1 IRQ number on STM32F401 */
#define SPI1_IRQn            35U

#endif /* SPI_PRIVATE_H */
