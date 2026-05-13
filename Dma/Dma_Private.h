/**
 * Dma_Private.h — DMA1 register definitions for STM32F401
 */

#ifndef DMA_PRIVATE_H
#define DMA_PRIVATE_H

#include "Std_Types.h"

/* DMA Stream register map */
typedef struct {
    volatile uint32 CR;      /* 0x00 - Stream configuration   */
    volatile uint32 NDTR;    /* 0x04 - Number of data         */
    volatile uint32 PAR;     /* 0x08 - Peripheral address     */
    volatile uint32 M0AR;    /* 0x0C - Memory 0 address       */
    volatile uint32 M1AR;    /* 0x10 - Memory 1 address       */
    volatile uint32 FCR;     /* 0x14 - FIFO control           */
} DmaStreamType;

/* DMA controller register map */
typedef struct {
    volatile uint32 LISR;    /* 0x00 - Low interrupt status   */
    volatile uint32 HISR;    /* 0x04 - High interrupt status  */
    volatile uint32 LIFCR;   /* 0x08 - Low interrupt flag clr */
    volatile uint32 HIFCR;   /* 0x0C - High interrupt flag clr*/
    DmaStreamType   Stream[8]; /* 0x10 + N*0x18               */
} DmaType;

#define DMA1_BASE_ADDR      0x40026000UL
#define DMA1                 ((DmaType *)DMA1_BASE_ADDR)

/* We use Stream 6 for USART2_TX */
#define DMA_STREAM_UART_TX   6U

/* CR bit positions */
#define DMA_CR_EN            0U      /* Stream enable               */
#define DMA_CR_TCIE          4U      /* Transfer complete IE        */
#define DMA_CR_DIR0          6U      /* Direction [7:6]: 01=mem2per */
#define DMA_CR_MINC          10U     /* Memory increment            */
#define DMA_CR_CHSEL0        25U     /* Channel select [27:25]      */

/* HISR / HIFCR bit positions for Stream 6 */
#define DMA_HISR_TCIF6       21U     /* TC flag for stream 6        */
#define DMA_HISR_HTIF6       20U
#define DMA_HISR_TEIF6       19U
#define DMA_HISR_DMEIF6      18U
#define DMA_HISR_FEIF6       16U

/* DMA1 Stream 6 IRQ number */
#define DMA1_STREAM6_IRQn    17U

/* Channel 4 for USART2_TX: bits [27:25] = 100 */
#define DMA_CHANNEL_4        (4UL << DMA_CR_CHSEL0)

#endif /* DMA_PRIVATE_H */
