/*
 ******************************************************************************
 * @file           : lcd.h
 * @brief          : 16x2 HD44780 LCD driver (4-bit mode) interface
 * @author         : Group Project — SNU ECE 2025
 * 
 * Provides API for initializing and controlling a 16x2 character LCD display
 * in 4-bit mode via GPIO. Pin definitions are centralized in config.h.
 * Supports text output with line addressing (1-based).
 ******************************************************************************
 */

#ifndef LCD_H
#define LCD_H

#include "stm32f3xx_hal.h"
#include "config.h"
#include <stdint.h>

/* ============================================================================
 * Pin Definitions (using config.h for consistency)
 * ============================================================================ */

#define LCD_RS_PORT     LCD_RS_GPIO_Port
#define LCD_RS_PIN      LCD_RS_Pin

#define LCD_EN_PORT     LCD_EN_GPIO_Port
#define LCD_EN_PIN      LCD_EN_Pin

#define LCD_D4_PORT     LCD_D4_GPIO_Port
#define LCD_D4_PIN      LCD_D4_Pin

#define LCD_D5_PORT     LCD_D5_GPIO_Port
#define LCD_D5_PIN      LCD_D5_Pin

#define LCD_D6_PORT     LCD_D6_GPIO_Port
#define LCD_D6_PIN      LCD_D6_Pin

#define LCD_D7_PORT     LCD_D7_GPIO_Port
#define LCD_D7_PIN      LCD_D7_Pin

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * lcd_init
 * @brief Initialize LCD display (4-bit mode, 2 lines)
 * Performs power-up sequence and configures display for operation
 * @param None
 * @return void
 */
void lcd_init(void);

/**
 * lcd_clear
 * @brief Clear display and return cursor to home position
 * @param None
 * @return void
 */
void lcd_clear(void);

/**
 * lcd_print
 * @brief Print string at current cursor position
 * String is truncated if longer than LCD_LINE_MAX_CHARS
 * @param str: Null-terminated string to print
 * @return void
 */
void lcd_print(const char *str);

/**
 * lcd_print_line
 * @brief Print string at start of specified line (1 or 2)
 * Line 1 is first row, Line 2 is second row on 16x2 display
 * String is truncated if longer than LCD_LINE_MAX_CHARS
 * @param line: Line number (1 or 2)
 * @param str: Null-terminated string to print
 * @return void (silently ignores invalid line number)
 */
void lcd_print_line(uint8_t line, const char *str);

#endif /* LCD_H */
