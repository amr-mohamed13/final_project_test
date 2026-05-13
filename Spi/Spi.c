/**
 * Spi.c — SPI1 Full-Duplex Driver for STM32F401
 *
 * Master: software NSS, byte-by-byte exchange with timeout.
 * Slave:  hardware NSS, RXNE interrupt-driven, pre-loaded TX.
 */

#include "Spi.h"
#include "Spi_Private.h"
#include "Bit_Operations.h"
#include "Nvic.h"
#include "Gpio.h"
#include "system_defs.h"

/* ------------------------------------------------------------------ */
/*  Slave internal buffers                                            */
/* ------------------------------------------------------------------ */
static volatile uint8 s_slaveTxBuf[SPI_FRAME_LEN];
static volatile uint8 s_slaveRxBuf[SPI_FRAME_LEN];
static volatile uint8 s_slaveTxIdx;
static volatile uint8 s_slaveRxIdx;
static volatile uint8 s_slaveRxReady;

/* Simple spin-timeout (~1ms at 16MHz) */
#define SPI_TIMEOUT_LOOPS   16000UL

/* ================================================================== */
/*  MASTER                                                            */
/* ================================================================== */

void Spi_InitMaster(void) {
    /* Reset CR1 */
    SPI1->CR1 = 0;

    /*
     * MSTR   = 1   (Master mode)
     * SSM    = 1   (Software slave management)
     * SSI    = 1   (Internal NSS high — prevents MODF)
     * BR     = 101 (fPCLK/64 = 16MHz/64 = 250 kHz)
     *               Slower clock gives Slave ISR time to respond
     * CPOL=0, CPHA=0, DFF=0 (8-bit)
     */
    SPI1->CR1 = (1UL << SPI_CR1_MSTR)
              | (1UL << SPI_CR1_SSM)
              | (1UL << SPI_CR1_SSI)
              | (5UL << SPI_CR1_BR0);   /* BR = 101 → /64 */

    SPI1->CR2 = 0;

    /* CS idle high */
    Gpio_WritePin(PIN_SPI_CS_PORT, PIN_SPI_CS_PIN, HIGH);

    /* Enable SPI */
    SET_BIT(SPI1->CR1, SPI_CR1_SPE);
}

/* Small delay (~5µs at 16MHz) to let Slave ISR execute */
static void Spi_InterByteDelay(void) {
    volatile uint8 d;
    for (d = 0; d < 20; d++) { }
}

uint8 Spi_MasterExchange(const uint8 *txBuf, uint8 *rxBuf, uint8 len) {
    uint32 timeout;

    /* Assert CS (active low) */
    Gpio_WritePin(PIN_SPI_CS_PORT, PIN_SPI_CS_PIN, LOW);

    /* Settling delay — let Slave NSS hardware detect selection */
    Spi_InterByteDelay();

    for (uint8 i = 0; i < len; i++) {
        /* Wait for TXE */
        timeout = SPI_TIMEOUT_LOOPS;
        while (!READ_BIT(SPI1->SR, SPI_SR_TXE)) {
            if (--timeout == 0) {
                Gpio_WritePin(PIN_SPI_CS_PORT, PIN_SPI_CS_PIN, HIGH);
                return 1; /* Timeout */
            }
        }

        SPI1->DR = txBuf[i];

        /* Wait for RXNE */
        timeout = SPI_TIMEOUT_LOOPS;
        while (!READ_BIT(SPI1->SR, SPI_SR_RXNE)) {
            if (--timeout == 0) {
                Gpio_WritePin(PIN_SPI_CS_PORT, PIN_SPI_CS_PIN, HIGH);
                return 1; /* Timeout */
            }
        }

        rxBuf[i] = (uint8)SPI1->DR;

        /* Inter-byte gap: let Slave ISR fire and preload next TX byte */
        Spi_InterByteDelay();
    }

    /* Wait until not busy */
    timeout = SPI_TIMEOUT_LOOPS;
    while (READ_BIT(SPI1->SR, SPI_SR_BSY)) {
        if (--timeout == 0) break;
    }

    /* Deassert CS */
    Gpio_WritePin(PIN_SPI_CS_PORT, PIN_SPI_CS_PIN, HIGH);

    return 0; /* OK */
}

/* ================================================================== */
/*  SLAVE                                                             */
/* ================================================================== */

void Spi_InitSlave(void) {
    /* Reset CR1 */
    SPI1->CR1 = 0;

    /*
     * MSTR = 0 (Slave mode)
     * SSM  = 0 (Hardware NSS)
     * CPOL=0, CPHA=0, DFF=0 (8-bit)
     */
    SPI1->CR1 = 0;

    /* Enable RXNE interrupt */
    SPI1->CR2 = 0;
    SET_BIT(SPI1->CR2, SPI_CR2_RXNEIE);

    /* Enable SPI1 interrupt in NVIC */
    Nvic_SetPriority(SPI1_IRQn, 2U);
    Nvic_EnableIrq(SPI1_IRQn);

    /* Clear any pending data */
    (void)SPI1->DR;
    (void)SPI1->SR;

    s_slaveTxIdx  = 0;
    s_slaveRxIdx  = 0;
    s_slaveRxReady = 0;

    /* Enable SPI */
    SET_BIT(SPI1->CR1, SPI_CR1_SPE);
}

void Spi_SlavePreload(const uint8 *txBuf, uint8 len) {
    ENTER_CRITICAL();
    for (uint8 i = 0; i < len && i < SPI_FRAME_LEN; i++) {
        s_slaveTxBuf[i] = txBuf[i];
    }
    s_slaveTxIdx = 1;
    s_slaveRxIdx = 0;
    /* Preload first byte into DR so it's ready when Master clocks */
    SPI1->DR = s_slaveTxBuf[0];
    EXIT_CRITICAL();
}

uint8 Spi_SlaveRead(uint8 *rxBuf, uint8 maxLen) {
    if (!s_slaveRxReady) return 0;

    ENTER_CRITICAL();
    uint8 count = (maxLen < SPI_FRAME_LEN) ? maxLen : SPI_FRAME_LEN;
    for (uint8 i = 0; i < count; i++) {
        rxBuf[i] = s_slaveRxBuf[i];
    }
    s_slaveRxReady = 0;
    EXIT_CRITICAL();

    return count;
}

/* ================================================================== */
/*  SPI1 IRQ Handler (Slave mode RXNE)                                */
/* ================================================================== */
void SPI1_IRQHandler(void) {
    if (READ_BIT(SPI1->SR, SPI_SR_RXNE)) {
        uint8 rx = (uint8)SPI1->DR;

        if (s_slaveRxIdx < SPI_FRAME_LEN) {
            s_slaveRxBuf[s_slaveRxIdx++] = rx;
        }

        /* Load next TX byte */
        if (s_slaveTxIdx < SPI_FRAME_LEN) {
            SPI1->DR = s_slaveTxBuf[s_slaveTxIdx++];
        }

        /* Full frame received */
        if (s_slaveRxIdx >= SPI_FRAME_LEN) {
            s_slaveRxReady = 1;
            s_slaveRxIdx   = 0;
            s_slaveTxIdx   = 0;
            /* Don't preload here — NSS will go HIGH (deselect) soon
             * which can wipe DR. Main loop re-preloads after processing. */
            g_spiRxCompleteFlag = 1;
        }
    }

    /* Clear overrun if set */
    if (READ_BIT(SPI1->SR, SPI_SR_OVR)) {
        (void)SPI1->DR;
        (void)SPI1->SR;
    }
}
