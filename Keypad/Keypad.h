
#ifndef KEYPAD_H
#define KEYPAD_H

#include "Std_Types.h"
#include "Gpio.h"


/*Keypad 4x4 pin mapping*/
#define Keypad_R0   GPIO_D, 0
#define Keypad_R1   GPIO_D, 1
#define Keypad_R2   GPIO_D, 2
#define Keypad_R3   GPIO_D, 3

#define Keypad_C0   GPIO_D, 4
#define Keypad_C1   GPIO_D, 5
#define Keypad_C2   GPIO_D, 6
#define Keypad_C3   GPIO_D, 7

#define KEYPAD_NUM_ROWS   4
#define KEYPAD_NUM_COLS   4

#define KEYPAD_NO_KEY     0xFF


void Keypad_Init(void);

uint8 Keypad_Scan(void);

#endif //KEYPAD_H
