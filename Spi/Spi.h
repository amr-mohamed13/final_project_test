/**
 * Spi.h — SPI1 Full-Duplex Driver (Master + Slave)
 */

#ifndef SPI_H
#define SPI_H

#include "Std_Types.h"

/**
 * @brief  Initialise SPI1 as Master.
 *         Software NSS, 8-bit, CPOL=0/CPHA=0, ~1 MHz.
 *         GPIO AF must be configured BEFORE calling this.
 */
void Spi_InitMaster(void);

/**
 * @brief  Initialise SPI1 as Slave.
 *         Hardware NSS, 8-bit, CPOL=0/CPHA=0, RXNE interrupt enabled.
 */
void Spi_InitSlave(void);

/**
 * @brief  Master: Full-duplex exchange of len bytes.
 *         Asserts CS, clocks out txBuf while reading into rxBuf, deasserts CS.
 *         Blocks only for the duration of the transfer (~64µs for 8 bytes).
 * @return 0 = OK, 1 = timeout
 */
uint8 Spi_MasterExchange(const uint8 *txBuf, uint8 *rxBuf, uint8 len);

/**
 * @brief  Slave: Pre-load TX buffer so data is ready when Master clocks.
 *         Must be called BEFORE the next Master-initiated transfer.
 */
void Spi_SlavePreload(const uint8 *txBuf, uint8 len);

/**
 * @brief  Slave: Read the last received packet.
 * @return Number of bytes copied into rxBuf (0 if no new data).
 */
uint8 Spi_SlaveRead(uint8 *rxBuf, uint8 maxLen);

#endif /* SPI_H */
