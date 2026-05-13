/**
 * Uart.h — USART2 Driver (TX via DMA, Master only)
 */

#ifndef UART_H
#define UART_H

#include "Std_Types.h"

/**
 * @brief  Initialise USART2 for TX-only at the given baud rate.
 *         GPIO AF7 must be configured on PA2 (TX) before calling.
 * @param  BaudRate  e.g. 9600, 115200
 */
void Uart_Init(uint32 BaudRate);

/**
 * @brief  Transmit a buffer via DMA (zero CPU overhead).
 *         Returns immediately; DMA handles the transfer.
 * @param  buf  Pointer to data
 * @param  len  Number of bytes to send
 */
void Uart_SendDma(const uint8 *buf, uint16 len);

/**
 * @brief  Transmit a buffer byte-by-byte (blocking, no DMA).
 *         Use this as a fallback when DMA is not available
 *         (e.g. Proteus simulation).
 * @param  buf  Pointer to data
 * @param  len  Number of bytes to send
 */
void Uart_Send(const uint8 *buf, uint16 len);

/**
 * @brief  Check if the previous DMA transfer is complete.
 * @return 1 = ready for new transfer, 0 = busy
 */
uint8 Uart_IsTxReady(void);

#endif /* UART_H */
