
#include "Keypad.h"
#include "Gpio.h"
#include "Utils.h"


uint8 KeypadDataMap[KEYPAD_NUM_ROWS][KEYPAD_NUM_COLS] = {
    {'7', '8', '9', 'C'},
    {'4', '5', '6', 'B'},
    {'1', '2', '3', 'A'},
    {'*', '0', '#', 'D'}
};

uint8 KeypadRowsPins[KEYPAD_NUM_ROWS][2] = {
    {Keypad_R0},
    {Keypad_R1},
    {Keypad_R2},
    {Keypad_R3}
};

uint8 KeypadColsPins[KEYPAD_NUM_COLS][2] = {
    {Keypad_C0},
    {Keypad_C1},
    {Keypad_C2},
    {Keypad_C3}
};


void Keypad_Init(void) {
    uint8 i = 0;

    // init rows as push-pull outputs
    for (i = 0; i < KEYPAD_NUM_ROWS; i++) {
        Gpio_Init(KeypadRowsPins[i][0], KeypadRowsPins[i][1], GPIO_OUTPUT, GPIO_PUSH_PULL);
    }

    // init cols as inputs with pull-up
    for (i = 0; i < KEYPAD_NUM_COLS; i++) {
        Gpio_Init(KeypadColsPins[i][0], KeypadColsPins[i][1], GPIO_INPUT, GPIO_PULL_UP);
    }

    // set all rows high (idle state)
    for (i = 0; i < KEYPAD_NUM_ROWS; i++) {
        Gpio_WritePin(KeypadRowsPins[i][0], KeypadRowsPins[i][1], HIGH);
    }
}


uint8 Keypad_Scan(void) {
    uint8 i = 0, j = 0, key = KEYPAD_NO_KEY;

    for (i = 0; i < KEYPAD_NUM_ROWS; i++) {
        // drive row low
        Gpio_WritePin(KeypadRowsPins[i][0], KeypadRowsPins[i][1], LOW);

        for (j = 0; j < KEYPAD_NUM_COLS; j++) {
            if (Gpio_ReadPin(KeypadColsPins[j][0], KeypadColsPins[j][1]) == LOW) {
                delay_ms(30); // debounce
                if (Gpio_ReadPin(KeypadColsPins[j][0], KeypadColsPins[j][1]) == LOW) {
                    key = KeypadDataMap[i][j];
                }
            }
        }

        // restore row high
        Gpio_WritePin(KeypadRowsPins[i][0], KeypadRowsPins[i][1], HIGH);
    }

    return key;
}
