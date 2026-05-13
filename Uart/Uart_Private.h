/**
 * Uart_Private.h — USART2 register definitions for STM32F401
 */

#ifndef UART_PRIVATE_H
#define UART_PRIVATE_H

#include "Std_Types.h"

typedef struct {
    volatile uint32 SR;      /* 0x00 - Status register        */
    volatile uint32 DR;      /* 0x04 - Data register          */
    volatile uint32 BRR;     /* 0x08 - Baud rate register     */
    volatile uint32 CR1;     /* 0x0C - Control register 1     */
    volatile uint32 CR2;     /* 0x10 - Control register 2     */
    volatile uint32 CR3;     /* 0x14 - Control register 3     */
    volatile uint32 GTPR;    /* 0x18 - Guard time / prescaler */
} UsartType;

#define USART2_BASE_ADDR    0x40004400UL
#define USART2              ((UsartType *)USART2_BASE_ADDR)

/* CR1 bit positions */
#define USART_CR1_UE        13U     /* USART enable                  */
#define USART_CR1_TE        3U      /* Transmitter enable            */
#define USART_CR1_RE        2U      /* Receiver enable               */
#define USART_CR1_RXNEIE    5U      /* RXNE interrupt enable         */
#define USART_CR1_TCIE      6U      /* Transmission complete IE      */
#define USART_CR1_TXEIE     7U      /* TXE interrupt enable          */

/* CR3 bit positions */
#define USART_CR3_DMAT      7U      /* DMA enable transmitter        */
#define USART_CR3_DMAR      6U      /* DMA enable receiver           */

/* SR bit positions */
#define USART_SR_TXE        7U      /* Transmit data register empty  */
#define USART_SR_TC         6U      /* Transmission complete         */
#define USART_SR_RXNE       5U      /* Read data register not empty  */

/* USART2 IRQ number */
#define USART2_IRQn         38U

/* System clock for baud rate calculation */
#define UART_PCLK1          16000000UL  /* APB1 clock = HSI 16 MHz   */

#endif /* UART_PRIVATE_H */
