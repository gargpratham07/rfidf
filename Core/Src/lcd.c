/*
 ******************************************************************************
 * @file           : lcd.c
 * @brief          : 16x2 HD44780 LCD driver (4-bit mode) implementation
 * @author         : Group Project — SNU ECE 2025
 * 
 * Implements 4-bit mode operation for a standard HD44780 16x2 character LCD.
 * Uses bit-banging over GPIO pins. Provides initialization, clear, and line-based
 * text output functions. All timing parameters defined in config.h.
 ******************************************************************************
 */

#include "lcd.h"
#include "config.h"
#include "stm32f3xx_hal.h"
#include <string.h>
#include <stdint.h>

/* ============================================================================
 * Private Helper Functions
 * ============================================================================ */

/**
 * lcd_udelay
 * @brief Soft delay using NOP instructions
 * Waits approximately 'cycles' iterations at current CPU speed
 * Used for precise LCD timing in nibble transmission
 * @param cycles: Number of NOP iterations to execute
 * @return void
 */
static void lcd_udelay(volatile uint32_t cycles)
{
    while (cycles--) {
        __asm__ volatile ("nop");
    }
}

/**
 * lcd_pulse_enable
 * @brief Generate E (enable) pulse to latch data into LCD
 * Toggles EN pin high then low with appropriate delays
 * @param None
 * @return void
 */
static void lcd_pulse_enable(void)
{
    HAL_GPIO_WritePin(LCD_EN_PORT, LCD_EN_PIN, GPIO_PIN_SET);
    lcd_udelay(LCD_ENABLE_PULSE_CYCLES);
    HAL_GPIO_WritePin(LCD_EN_PORT, LCD_EN_PIN, GPIO_PIN_RESET);
    lcd_udelay(LCD_ENABLE_PULSE_CYCLES);
}

/**
 * lcd_write_nibble
 * @brief Write 4-bit nibble to LCD data pins (D4-D7)
 * Splits 4-bit value across GPIO pins and generates enable pulse
 * Timing: maps bit0->D4, bit1->D5, bit2->D6, bit3->D7
 * @param nibble: 4-bit value (only lower 4 bits used)
 * @return void
 */
static void lcd_write_nibble(uint8_t nibble)
{
    nibble &= 0x0F;

    /* Bit-banging: map nibble bits to GPIO pins */
    HAL_GPIO_WritePin(LCD_D4_PORT, LCD_D4_PIN, (nibble & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D5_PORT, LCD_D5_PIN, (nibble & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D6_PORT, LCD_D6_PIN, (nibble & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D7_PORT, LCD_D7_PIN, (nibble & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    lcd_udelay(LCD_UDELAY_SHORT_CYCLES);
    lcd_pulse_enable();
}

/**
 * lcd_send_byte
 * @brief Send 8-bit command or data to LCD in 4-bit mode
 * Splits byte into two nibbles (high then low) and transmits with E pulse
 * @param byte: 8-bit value to transmit
 * @param isData: 1 if data byte, 0 if command byte (sets RS pin accordingly)
 * @return void
 */
static void lcd_send_byte(uint8_t byte, uint8_t isData)
{
    /* RS (Register Select): 1=data, 0=command */
    HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, isData ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* Send high nibble (bits 7-4) then low nibble (bits 3-0) */
    lcd_write_nibble((byte >> 4) & 0x0F);
    lcd_write_nibble(byte & 0x0F);

    lcd_udelay(LCD_UDELAY_MED_CYCLES);
}

/**
 * lcd_set_line
 * @brief Set cursor to start of specified line (1 or 2)
 * Line 1: DDRAM address 0x00, Line 2: DDRAM address 0x40
 * @param line: Line number (1 or 2)
 * @return void
 */
static void lcd_set_line(uint8_t line)
{
    uint8_t addr = (line == 1) ? 0x00 : 0x40;
    lcd_send_byte(0x80 | addr, 0);
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void lcd_init(void)
{
    /* Allow LCD to stabilize after power-on */
    HAL_Delay(LCD_POWERUP_DELAY_MS);

    /* Ensure control lines are in known state (low) */
    HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_EN_PORT, LCD_EN_PIN, GPIO_PIN_RESET);

    /* 4-bit mode initialization sequence: send 0x03 three times, then 0x02 */
    /* This is the standard HD44780 power-on sequence */
    lcd_write_nibble(0x03);
    HAL_Delay(5);
    lcd_write_nibble(0x03);
    HAL_Delay(5);
    lcd_write_nibble(0x03);
    HAL_Delay(2);
    lcd_write_nibble(0x02);
    HAL_Delay(2);

    /* Function Set: 4-bit interface, 2 lines, 5x8 font */
    lcd_send_byte(0x28, 0);
    /* Display Control: display ON, cursor OFF, blink OFF */
    lcd_send_byte(0x0C, 0);
    /* Entry Mode: increment address, shift display OFF */
    lcd_send_byte(0x06, 0);
    /* Clear display and return to home */
    lcd_send_byte(0x01, 0);
    HAL_Delay(LCD_GENERIC_DELAY_MS);
}

void lcd_clear(void)
{
    /* Send clear display command (0x01) */
    lcd_send_byte(0x01, 0);
    HAL_Delay(LCD_GENERIC_DELAY_MS);
}

void lcd_print(const char *str)
{
    /* Print string starting at current cursor position */
    if (!str) {
        return;
    }
    while (*str) {
        lcd_send_byte((uint8_t)(*str), 1);
        str++;
    }
}

void lcd_print_line(uint8_t line, const char *str)
{
    /* Prepare buffer with padding for 16-char line display */
    char buf[LCD_LINE_MAX_CHARS];
    size_t len = strlen(str);
    
    /* Clamp length to line width and fill with spaces */
    if (len > (LCD_LINE_MAX_CHARS - 1)) {
        len = (LCD_LINE_MAX_CHARS - 1);
    }
    
    /* Pad buffer with spaces */
    for (int i = 0; i < (LCD_LINE_MAX_CHARS - 1); ++i) {
        buf[i] = ' ';
    }
    
    /* Copy string into buffer */
    memcpy(buf, str, len);
    buf[LCD_LINE_MAX_CHARS - 1] = '\0';

    /* Position cursor at line start and output all 16 characters */
    lcd_set_line(line);
    HAL_Delay(1);
    for (int i = 0; i < (LCD_LINE_MAX_CHARS - 1); ++i) {
        lcd_send_byte((uint8_t)buf[i], 1);
    }
}

