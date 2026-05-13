/**
 * Lcd.h
 *
 *  Created on: 4/28/2026
 *  Description: Public API for HD44780-compatible 16x2 LCD in 4-bit parallel mode.
 *               No external LCD library is used — pure register-level GPIO driving.
 *
 *  Pin mapping (configured in Lcd.c):
 *    RS → PB0   (Register Select: 0=command, 1=data)
 *    EN → PB1   (Enable strobe)
 *    D4 → PB4
 *    D5 → PB5
 *    D6 → PB6
 *    D7 → PB7
 */

#ifndef LCD_H
#define LCD_H

#include "Std_Types.h"

/**
 * @brief  Initialise the LCD in 4-bit mode.
 *         Performs the full HD44780 power-on initialisation sequence.
 *         Must be called once after GPIO and RCC clocks for GPIOB are enabled.
 */
void Lcd_Init(void);

/**
 * @brief  Clear the display and return cursor to home (row 0, col 0).
 */
void Lcd_Clear(void);

/**
 * @brief  Move cursor to the specified position.
 * @param  Row  0 = top line, 1 = bottom line
 * @param  Col  0 – 15
 */
void Lcd_SetCursor(uint8 Row, uint8 Col);

/**
 * @brief  Write a single character at the current cursor position.
 * @param  c  ASCII character
 */
void Lcd_WriteChar(char c);

/**
 * @brief  Write a null-terminated string starting at the current cursor.
 * @param  str  Pointer to string
 */
void Lcd_WriteString(const char *str);

/**
 * @brief  Write a signed 32-bit integer as a decimal string.
 * @param  val  Value to display
 */
void Lcd_WriteInt(sint32 val);

#endif /* LCD_H */
