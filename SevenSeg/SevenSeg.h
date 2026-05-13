
#ifndef SEVENSEG_H
#define SEVENSEG_H

#include "Std_Types.h"
#include "Gpio.h"


/*7-Segment pin mapping (common-cathode, segments A-G on Port C)*/
#define SEGMENT_A   GPIO_C, 0
#define SEGMENT_B   GPIO_C, 1
#define SEGMENT_C   GPIO_C, 2
#define SEGMENT_D   GPIO_C, 3
#define SEGMENT_E   GPIO_C, 4
#define SEGMENT_F   GPIO_C, 5
#define SEGMENT_G   GPIO_C, 6

#define SEVENSEG_NUM_SEGMENTS   7

/*Max displayable value (failed attempts can reach 10)*/
#define SEVENSEG_MAX_VALUE   10


void SevenSeg_Init(void);

void SevenSeg_Display(uint8 number);

#endif //SEVENSEG_H
