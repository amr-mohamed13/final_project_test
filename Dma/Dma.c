/**
 * Dma.c — DMA1 Stream6 Channel4 for USART2 TX
 */

#include "Dma.h"
#include "Dma_Private.h"
#include "Bit_Operations.h"
#include "Nvic.h"
#include "Rcc.h"

void Dma_Init(void) {
    Rcc_Enable(RCC_DMA1);
}

void Dma_ConfigUartTx(uint32 periphAddr, uint32 memAddr, uint16 len) {
    DmaStreamType *stream = &DMA1->Stream[DMA_STREAM_UART_TX];

    /* Disable stream before configuring */
    CLEAR_BIT(stream->CR, DMA_CR_EN);
    while (READ_BIT(stream->CR, DMA_CR_EN)) { /* wait */ }

    /* Clear all interrupt flags for Stream 6 (write 1 to HIFCR bits) */
    DMA1->HIFCR = (1UL << DMA_HISR_TCIF6)
                | (1UL << DMA_HISR_HTIF6)
                | (1UL << DMA_HISR_TEIF6)
                | (1UL << DMA_HISR_DMEIF6)
                | (1UL << DMA_HISR_FEIF6);

    /* Peripheral address (USART2->DR) */
    stream->PAR  = periphAddr;
    /* Memory address (source buffer) */
    stream->M0AR = memAddr;
    /* Number of data items */
    stream->NDTR = len;

    /*
     * CR configuration:
     *   Channel 4        (USART2_TX)
     *   DIR = 01          (Memory → Peripheral)
     *   MINC = 1          (Memory increment)
     *   TCIE = 1          (Transfer complete interrupt)
     *   Data size = byte  (default 00)
     */
    stream->CR = DMA_CHANNEL_4
               | (1UL << DMA_CR_DIR0)   /* DIR = 01: mem-to-periph */
               | (1UL << DMA_CR_MINC)
               | (1UL << DMA_CR_TCIE);

    /* Enable DMA1_Stream6 IRQ in NVIC */
    Nvic_SetPriority(DMA1_STREAM6_IRQn, 3U);
    Nvic_EnableIrq(DMA1_STREAM6_IRQn);
}

void Dma_Start(void) {
    SET_BIT(DMA1->Stream[DMA_STREAM_UART_TX].CR, DMA_CR_EN);
}

/* DMA1 Stream6 IRQ — Transfer Complete */
void DMA1_Stream6_IRQHandler(void) {
    if (READ_BIT(DMA1->HISR, DMA_HISR_TCIF6)) {
        /* Clear TC flag */
        DMA1->HIFCR = (1UL << DMA_HISR_TCIF6);

        /* Disable stream */
        CLEAR_BIT(DMA1->Stream[DMA_STREAM_UART_TX].CR, DMA_CR_EN);

        /* Notify UART driver */
        Uart_DmaComplete();
    }
}
