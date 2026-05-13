
#ifndef GPIO_H
#define GPIO_H

#include "Std_Types.h"


/*PortName*/
#define GPIO_A     'A'
#define GPIO_B     'B'
#define GPIO_C     'C'
#define GPIO_D     'D'


/*PinMode*/
#define GPIO_INPUT  0x00
#define GPIO_OUTPUT 0x01
#define GPIO_AF     0x02
#define GPIO_ANALOG 0x03

/*DefaultState*/
#define GPIO_PUSH_PULL  0x00
#define GPIO_OPEN_DRAIN 0x01

#define GPIO_NO_PULL_DOWN 0x00
#define GPIO_PULL_UP      0x01
#define GPIO_PULL_DOWN    0x02

/*AlternateFunction values (AF0..AF9 used on STM32F401)*/
#define GPIO_AF0   0x00
#define GPIO_AF1   0x01
#define GPIO_AF2   0x02
#define GPIO_AF3   0x03
#define GPIO_AF4   0x04
#define GPIO_AF5   0x05
#define GPIO_AF6   0x06
#define GPIO_AF7   0x07
#define GPIO_AF8   0x08
#define GPIO_AF9   0x09

/*Data*/
#define LOW      0x0
#define HIGH     0x1


#define OK  0x0
#define NOK 0x1


void Gpio_Init(uint8 PortName, uint8 PinNumber, uint8 PinMode, uint8 DefaultState);

uint8 Gpio_WritePin(uint8 PortName, uint8 PinNumber, uint8 Data);

uint8 Gpio_ReadPin(uint8 PortName, uint8 PinNumber);

/**
 * @brief  Configure the alternate-function selection for a pin.
 *         Must be called after Gpio_Init() with PinMode = GPIO_AF.
 * @param  PortName    GPIO_A .. GPIO_D
 * @param  PinNumber   0 .. 15
 * @param  AfValue     GPIO_AF0 .. GPIO_AF9
 */
void Gpio_SetAF(uint8 PortName, uint8 PinNumber, uint8 AfValue);

#endif //GPIO_H
