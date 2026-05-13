#include "Exti.h"
#include "Exti_Private.h" 

void Exti_Init(uint8 PortSource, uint8 PinNumber, uint8 TriggerEdge, uint8 NvicPriority) {
    uint8 crIndex  = PinNumber % 4U;   /* position within the EXTICR register   */
    uint8 crReg    = PinNumber / 4U;   /* which EXTICR[0..3]                     */
    EXTI_IRQn_t irqn;

    /* Determine IRQ number */
    if      (PinNumber == 0U) { irqn = EXTI0_IRQn;      }
    else if (PinNumber == 1U) { irqn = EXTI1_IRQn;      }
    else if (PinNumber == 2U) { irqn = EXTI2_IRQn;      }
    else if (PinNumber == 3U) { irqn = EXTI3_IRQn;      }
    else if (PinNumber == 4U) { irqn = EXTI4_IRQn;      }
    else if (PinNumber <= 9U) { irqn = EXTI9_5_IRQn;    }
    else                      { irqn = EXTI15_10_IRQn;  }

    /* 1. Route the correct GPIO port to this EXTI line via SYSCFG_EXTICRx */
    SYSCFG->EXTICR[crReg] &= ~(0x0FUL << (crIndex * 4U));
    SYSCFG->EXTICR[crReg] |=  ((uint32_t)PortSource << (crIndex * 4U));

    /* 2. Select trigger edge */
    EXTI->RTSR &= ~(1UL << PinNumber);
    EXTI->FTSR &= ~(1UL << PinNumber);
    if (TriggerEdge == EXTI_RISING  || TriggerEdge == EXTI_BOTH) {
        EXTI->RTSR |= (1UL << PinNumber);
    }
    if (TriggerEdge == EXTI_FALLING || TriggerEdge == EXTI_BOTH) {
        EXTI->FTSR |= (1UL << PinNumber);
    }

    /* 3. Unmask the EXTI interrupt line */
    EXTI->IMR |= (1UL << PinNumber);

    /* 4. Set priority and enable the interrupt via raw NVIC registers */
    NVIC->IP[(uint32_t)irqn] = (uint8_t)(NvicPriority << 4U);  /* upper nibble holds priority */
    NVIC->ISER[(uint32_t)irqn >> 5U] = (1UL << ((uint32_t)irqn & 0x1FUL));
}


void Exti_EnableLine(uint8 LineNumber) {
    EXTI->IMR |= (1UL << LineNumber);
}

void Exti_DisableLine(uint8 LineNumber) {
    EXTI->IMR &= ~(1UL << LineNumber);
}

void Exti_ClearPending(uint8 LineNumber) {
    /* Pending Register is cleared by writing 1 to the bit */
    EXTI->PR = (1UL << LineNumber);
}
