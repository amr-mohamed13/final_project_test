/**
 * Dma.h — DMA1 Driver for USART2 TX (Master only)
 */

#ifndef DMA_H
#define DMA_H

#include "Std_Types.h"

/** Initialise DMA1 clock. */
void Dma_Init(void);

/**
 * @brief Configure DMA1 Stream6 Ch4 for USART2_TX.
 * @param periphAddr  Address of USART2->DR
 * @param memAddr     Address of source buffer
 * @param len         Number of bytes
 */
void Dma_ConfigUartTx(uint32 periphAddr, uint32 memAddr, uint16 len);

/** Start the configured DMA transfer. */
void Dma_Start(void);

/* Called from Uart.c — DMA completion callback */
extern void Uart_DmaComplete(void);

#endif /* DMA_H */
