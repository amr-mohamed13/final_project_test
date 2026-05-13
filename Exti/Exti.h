#ifndef EXTI_H
#define EXTI_H

#include "Std_Types.h"


/*TriggerEdge*/
#define EXTI_RISING   0x00
#define EXTI_FALLING  0x01
#define EXTI_BOTH     0x02

/*PortSource for SYSCFG EXTICR (maps to SYSCFG port codes)*/
#define EXTI_PORT_A   0x00
#define EXTI_PORT_B   0x01
#define EXTI_PORT_C   0x02
#define EXTI_PORT_D   0x03


void Exti_Init(uint8 PortSource, uint8 PinNumber, uint8 TriggerEdge, uint8 NvicPriority);

void Exti_EnableLine(uint8 LineNumber);

void Exti_DisableLine(uint8 LineNumber);

void Exti_ClearPending(uint8 LineNumber);

#endif //EXTI_H
