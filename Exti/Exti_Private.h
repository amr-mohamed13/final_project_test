#ifndef EXTI_PRIVATE_H
#define EXTI_PRIVATE_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* EXTI registers                                                       */
/* ------------------------------------------------------------------ */
typedef struct {
    volatile uint32_t IMR;    /* 0x00 – Interrupt mask            */
    volatile uint32_t EMR;    /* 0x04 – Event mask                */
    volatile uint32_t RTSR;   /* 0x08 – Rising-trigger selection  */
    volatile uint32_t FTSR;   /* 0x0C – Falling-trigger selection */
    volatile uint32_t SWIER;  /* 0x10 – Software interrupt event  */
    volatile uint32_t PR;     /* 0x14 – Pending register          */
} EXTI_TypeDef;

/* ------------------------------------------------------------------ */
/* SYSCFG registers                                                     */
/* ------------------------------------------------------------------ */
typedef struct {
    volatile uint32_t MEMRMP;     /* 0x00 */
    volatile uint32_t PMC;        /* 0x04 */
    volatile uint32_t EXTICR[4];  /* 0x08 – 0x14 */
    volatile uint32_t CMPCR;      /* 0x20 */
} SYSCFG_TypeDef;

/* ------------------------------------------------------------------ */
/* NVIC registers (Cortex-M4, offsets from 0xE000E100)                 */
/* ------------------------------------------------------------------ */
typedef struct {
    volatile uint32_t ISER[8];        /* 0x000 – Interrupt Set-Enable   */
    volatile uint32_t RESERVED0[24];
    volatile uint32_t ICER[8];        /* 0x080 – Interrupt Clear-Enable */
    volatile uint32_t RESERVED1[24];
    volatile uint32_t ISPR[8];        /* 0x100 – Interrupt Set-Pending  */
    volatile uint32_t RESERVED2[24];
    volatile uint32_t ICPR[8];        /* 0x180 – Interrupt Clear-Pending*/
    volatile uint32_t RESERVED3[24];
    volatile uint32_t IABR[8];        /* 0x200 – Interrupt Active Bit   */
    volatile uint32_t RESERVED4[56];
    volatile uint8_t  IP[240];        /* 0x300 – Interrupt Priority     */
} NVIC_TypeDef;

/* ------------------------------------------------------------------ */
/* Base addresses                                                       */
/* ------------------------------------------------------------------ */
#define EXTI    ((EXTI_TypeDef *)   0x40013C00UL)
#define SYSCFG  ((SYSCFG_TypeDef *) 0x40013800UL)
#define NVIC    ((NVIC_TypeDef *)   0xE000E100UL)

/* ------------------------------------------------------------------ */
/* EXTI IRQ numbers (STM32F401 position in the NVIC vector table)       */
/* ------------------------------------------------------------------ */
typedef enum {
    EXTI0_IRQn      = 6,
    EXTI1_IRQn      = 7,
    EXTI2_IRQn      = 8,
    EXTI3_IRQn      = 9,
    EXTI4_IRQn      = 10,
    EXTI9_5_IRQn    = 23,
    EXTI15_10_IRQn  = 40
} EXTI_IRQn_t;

#endif /* EXTI_PRIVATE_H */
