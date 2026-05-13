
#include "SevenSeg.h"
#include "Rcc.h"


uint8 sevenSegmentConnectionMap[SEVENSEG_NUM_SEGMENTS][2] = {
    {SEGMENT_A},
    {SEGMENT_B},
    {SEGMENT_C},
    {SEGMENT_D},
    {SEGMENT_E},
    {SEGMENT_F},
    {SEGMENT_G}
};

/*
 * Common-cathode segment encoding for 0-10.
 * Bit order: G F E D C B A  (bit 0 = segment A)
 * 0x3F = 0b0111111 -> 0
 * 0x06 = 0b0000110 -> 1
 * 0x5B = 0b1011011 -> 2
 * 0x4F = 0b1001111 -> 3
 * 0x66 = 0b1100110 -> 4
 * 0x6D = 0b1101101 -> 5
 * 0x7D = 0b1111101 -> 6
 * 0x07 = 0b0000111 -> 7
 * 0x7F = 0b1111111 -> 8
 * 0x6F = 0b1101111 -> 9
 * 0x37 = 0b0110111 -> displays "10" as capital "I" (segments B,C,E,F on)
 */
const uint8 sevenSegmentTableMap[11] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,
    0x6D, 0x7D, 0x07, 0x7F, 0x6F,
    0x37
};


void SevenSeg_Init(void) {
    uint8 i = 0;
    for (i = 0; i < SEVENSEG_NUM_SEGMENTS; i++) {
        Gpio_Init(sevenSegmentConnectionMap[i][0], sevenSegmentConnectionMap[i][1],
                  GPIO_OUTPUT, GPIO_PUSH_PULL);
    }
}

void SevenSeg_Display(uint8 number) {
    uint8 i = 0, value = 0;

    if (number > SEVENSEG_MAX_VALUE) {
        return;
    }

    for (i = 0; i < SEVENSEG_NUM_SEGMENTS; i++) {
        value = (sevenSegmentTableMap[number] >> i) & 0x01U;
        Gpio_WritePin(sevenSegmentConnectionMap[i][0], sevenSegmentConnectionMap[i][1], value);
    }
}
