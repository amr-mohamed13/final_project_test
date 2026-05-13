/**
 * Uart.c — USART2 Driver with DMA TX for STM32F401
 */

#include "Uart.h"
#include "Uart_Private.h"
#include "Dma.h"
#include "Bit_Operations.h"

static volatile uint8 s_txBusy = 0;

void Uart_Init(uint32 BaudRate) {
    /* Reset control registers */
    USART2->CR1 = 0;
    USART2->CR2 = 0;
    USART2->CR3 = 0;

    /*
     * BRR = fPCLK1 / BaudRate  (fixed-point, see RM0368 §19.3.4)
     * At 16 MHz, 9600 baud → BRR ≈ 1667 (0x683)
     * Round instead of truncate for accuracy.
     */
    USART2->BRR = (uint16)((UART_PCLK1 + (BaudRate / 2U)) / BaudRate);

    /* Enable transmitter + USART */
    SET_BIT(USART2->CR1, USART_CR1_TE);
    SET_BIT(USART2->CR1, USART_CR1_UE);

    s_txBusy = 0;
}

void Uart_SendDma(const uint8 *buf, uint16 len) {
    if (s_txBusy) return;
    s_txBusy = 1;

    /* Enable DMA mode on USART2 */
    SET_BIT(USART2->CR3, USART_CR3_DMAT);

    /* Configure DMA1 Stream6 Channel4 for USART2_TX */
    Dma_ConfigUartTx((uint32)&USART2->DR, (uint32)buf, len);
    Dma_Start();
}

void Uart_Send(const uint8 *buf, uint16 len) {
    for (uint16 i = 0; i < len; i++) {
        /* Wait until TXE (transmit data register empty) */
        while (!READ_BIT(USART2->SR, USART_SR_TXE)) { /* spin */ }
        USART2->DR = buf[i];
    }
    /* Wait until TC (transmission complete) */
    while (!READ_BIT(USART2->SR, USART_SR_TC)) { /* spin */ }
}

uint8 Uart_IsTxReady(void) {
    return (s_txBusy == 0) ? 1U : 0U;
}

/* Called by DMA TC ISR to mark transfer complete */
void Uart_DmaComplete(void) {
    s_txBusy = 0;
}
