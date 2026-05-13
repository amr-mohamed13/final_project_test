/**
 * Lcd.c
 *
 *  Created on: 4/28/2026
 *  Description: 4-bit parallel driver for HD44780-based 16x2 LCD (LCD1602).
 *               Drives RS, EN, D4–D7 directly via the GPIO driver.
 *               Uses delay_ms() from Utils for init timing.
 *
 *  Anti-flicker strategy:
 *    The driver keeps a shadow copy of what is currently on the LCD.
 *    Lcd_WriteString() skips writing characters that have not changed.
 *    This eliminates the visible flicker that occurs when the entire
 *    display is cleared and rewritten on every ADC cycle.
 *
 *  Pin assignment (all on GPIOB):
 *    PB0 → RS     PB1 → EN
 *    PB4 → D4     PB5 → D5     PB6 → D6     PB7 → D7
 */

#include "Lcd.h"
#include "Gpio.h"
#include "Utils.h"
#include "Bit_Operations.h"
#include "Std_Types.h"

/* ------------------------------------------------------------------ */
/*  Pin definitions                                                     */
/* ------------------------------------------------------------------ */
#define LCD_PORT   GPIO_B

#define LCD_RS_PIN  0U
#define LCD_EN_PIN  1U
#define LCD_D4_PIN  4U
#define LCD_D5_PIN  5U
#define LCD_D6_PIN  6U
#define LCD_D7_PIN  7U

/* ------------------------------------------------------------------ */
/*  HD44780 command constants                                           */
/* ------------------------------------------------------------------ */
#define LCD_CMD_CLEAR_DISPLAY   0x01U
#define LCD_CMD_RETURN_HOME     0x02U
#define LCD_CMD_ENTRY_MODE      0x06U   /* Cursor increment, no display shift */
#define LCD_CMD_DISPLAY_ON      0x0CU   /* Display on, cursor off, blink off  */
#define LCD_CMD_FUNCTION_4BIT   0x28U   /* 4-bit, 2 lines, 5x8 font           */

#define LCD_DDRAM_ROW0          0x80U   /* DDRAM address for row 0            */
#define LCD_DDRAM_ROW1          0xC0U   /* DDRAM address for row 1            */

/* ------------------------------------------------------------------ */
/*  Anti-flicker: shadow display buffer                                */
/*  lcd_shadow[row][col] holds the last character written to that cell */
/* ------------------------------------------------------------------ */
#define LCD_ROWS  2U
#define LCD_COLS  16U

static char lcd_shadow[LCD_ROWS][LCD_COLS];

/* Current logical cursor position */
static uint8 lcd_cur_row = 0U;
static uint8 lcd_cur_col = 0U;

/* ------------------------------------------------------------------ */
/*  Low-level helpers                                                   */
/* ------------------------------------------------------------------ */

/** Set or clear a single LCD data/control pin */
static void Lcd_SetPin(uint8 Pin, uint8 Val) {
    Gpio_WritePin(LCD_PORT, Pin, Val);
}

/** Pulse the EN pin to clock data into the LCD controller */
static void Lcd_Pulse(void) {
    Lcd_SetPin(LCD_EN_PIN, HIGH);
    delay_ms(1U);
    Lcd_SetPin(LCD_EN_PIN, LOW);
    delay_ms(1U);
}

/**
 * Send a 4-bit nibble on D7..D4.
 * @param  nibble  Upper 4 bits are ignored; lower 4 bits are sent on D4–D7.
 */
static void Lcd_SendNibble(uint8 nibble) {
    Lcd_SetPin(LCD_D4_PIN, (nibble >> 0U) & 0x01U);
    Lcd_SetPin(LCD_D5_PIN, (nibble >> 1U) & 0x01U);
    Lcd_SetPin(LCD_D6_PIN, (nibble >> 2U) & 0x01U);
    Lcd_SetPin(LCD_D7_PIN, (nibble >> 3U) & 0x01U);
    Lcd_Pulse();
}

/**
 * Send a full byte to the LCD in two nibbles (high nibble first).
 * @param  byte    The command or data byte.
 * @param  isData  0 = command (RS=0), 1 = character data (RS=1)
 */
static void Lcd_SendByte(uint8 byte, uint8 isData) {
    Lcd_SetPin(LCD_RS_PIN, isData);
    Lcd_SendNibble(byte >> 4U);     /* High nibble */
    Lcd_SendNibble(byte & 0x0FU);   /* Low  nibble */
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

void Lcd_Init(void) {
    uint8 row, col;

    /* Initialise shadow buffer to spaces */
    for (row = 0U; row < LCD_ROWS; row++) {
        for (col = 0U; col < LCD_COLS; col++) {
            lcd_shadow[row][col] = ' ';
        }
    }
    lcd_cur_row = 0U;
    lcd_cur_col = 0U;

    /* GPIO pins must already be clocked & mode-set by App_Init() */

    /* HD44780 power-on sequence (datasheet §4.1.2 for 4-bit mode) */
    delay_ms(50U);                          /* Wait >40 ms after VCC rise  */

    Lcd_SetPin(LCD_RS_PIN, LOW);
    Lcd_SetPin(LCD_EN_PIN, LOW);

    /* Step 1: send 0x3 three times (8-bit interface reset) */
    Lcd_SendNibble(0x03U); delay_ms(5U);
    Lcd_SendNibble(0x03U); delay_ms(1U);
    Lcd_SendNibble(0x03U); delay_ms(1U);

    /* Step 2: switch to 4-bit interface */
    Lcd_SendNibble(0x02U); delay_ms(1U);

    /* Step 3: configure display */
    Lcd_SendByte(LCD_CMD_FUNCTION_4BIT, 0U); delay_ms(1U);  /* 2 lines, 5×8 */
    Lcd_SendByte(LCD_CMD_DISPLAY_ON,    0U); delay_ms(1U);  /* Display on    */
    Lcd_SendByte(LCD_CMD_CLEAR_DISPLAY, 0U); delay_ms(2U);  /* Clear         */
    Lcd_SendByte(LCD_CMD_ENTRY_MODE,    0U); delay_ms(1U);  /* Entry mode    */
}

void Lcd_Clear(void) {
    uint8 row, col;

    Lcd_SendByte(LCD_CMD_CLEAR_DISPLAY, 0U);
    delay_ms(2U);

    /* Reset shadow buffer */
    for (row = 0U; row < LCD_ROWS; row++) {
        for (col = 0U; col < LCD_COLS; col++) {
            lcd_shadow[row][col] = ' ';
        }
    }
    lcd_cur_row = 0U;
    lcd_cur_col = 0U;
}

void Lcd_SetCursor(uint8 Row, uint8 Col) {
    uint8 addr = (Row == 0U) ? LCD_DDRAM_ROW0 : LCD_DDRAM_ROW1;
    addr += Col;
    Lcd_SendByte(addr, 0U);
    delay_ms(1U);
    lcd_cur_row = Row;
    lcd_cur_col = Col;
}

void Lcd_WriteChar(char c) {
    if (lcd_cur_row >= LCD_ROWS || lcd_cur_col >= LCD_COLS) {
        return;
    }

    /* Anti-flicker: only write to the bus if character changed */
    if (lcd_shadow[lcd_cur_row][lcd_cur_col] != c) {
        lcd_shadow[lcd_cur_row][lcd_cur_col] = c;
        Lcd_SendByte((uint8) c, 1U);
        delay_ms(1U);
        lcd_cur_col++;
    } else {
        /* Character unchanged — advance logical col, then physically
         * reposition the HD44780 cursor so next write lands correctly. */
        lcd_cur_col++;
        /* Reposition only if still within the row (avoids a pointless command at EOL) */
        if (lcd_cur_col < LCD_COLS) {
            uint8 addr = (lcd_cur_row == 0U) ? LCD_DDRAM_ROW0 : LCD_DDRAM_ROW1;
            addr += lcd_cur_col;
            Lcd_SendByte(addr, 0U);  /* RS=0: DDRAM address set */
            delay_ms(1U);
        }
    }
}

void Lcd_WriteString(const char *str) {
    while (*str != '\0') {
        Lcd_WriteChar(*str);
        str++;
    }
}

void Lcd_WriteInt(sint32 val) {
    char  buf[12];
    uint8 idx = 0U;
    uint8 i, tmp;
    uint32 uval;

    if (val < 0) {
        buf[idx++] = '-';
        uval = (uint32)(-val);
    } else {
        uval = (uint32) val;
    }

    if (uval == 0U) {
        buf[idx++] = '0';
    } else {
        uint8 digitStart = idx;
        while (uval > 0U) {
            buf[idx++] = (char)('0' + (uval % 10U));
            uval /= 10U;
        }
        /* Reverse digit characters (they are stored least-significant first) */
        for (i = digitStart; i < (digitStart + (idx - digitStart) / 2U); i++) {
            tmp = buf[i];
            buf[i] = buf[idx - 1U - (i - digitStart)];
            buf[idx - 1U - (i - digitStart)] = tmp;
        }
    }

    buf[idx] = '\0';
    Lcd_WriteString(buf);
}
