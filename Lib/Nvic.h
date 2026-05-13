#ifndef NVIC_H
#define NVIC_H

#include "Std_Types.h"

/*
 * Lightweight NVIC helpers — raw register access.
 * NVIC base = 0xE000E100 (same struct as in Exti_Private.h)
 */

#define NVIC_ISER_BASE   ((volatile uint32 *)0xE000E100UL)
#define NVIC_ICER_BASE   ((volatile uint32 *)0xE000E180UL)
#define NVIC_IPR_BASE    ((volatile uint8  *)0xE000E400UL)

static inline void Nvic_EnableIrq(uint8 IrqNum) {
    NVIC_ISER_BASE[IrqNum >> 5U] = (1UL << (IrqNum & 0x1FU));
}

static inline void Nvic_DisableIrq(uint8 IrqNum) {
    NVIC_ICER_BASE[IrqNum >> 5U] = (1UL << (IrqNum & 0x1FU));
}

static inline void Nvic_SetPriority(uint8 IrqNum, uint8 Priority) {
    NVIC_IPR_BASE[IrqNum] = (uint8)(Priority << 4U);
}

#endif /* NVIC_H */
