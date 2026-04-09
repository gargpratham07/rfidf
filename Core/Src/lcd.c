/* Core/Src/lcd.c - robust 4-bit HD44780 driver for STM32F3 (HAL) */

#include "lcd.h"
#include "stm32f3xx_hal.h"
#include <string.h>
#include <stdint.h>

/* tiny busy-wait loop */
static void lcd_udelay(volatile uint32_t cycles)
{
    while (cycles--) { __asm__ volatile ("nop"); }
}

/* Pulse E (enable) */
static void lcd_pulse_enable(void)
{
    HAL_GPIO_WritePin(LCD_EN_PORT, LCD_EN_PIN, GPIO_PIN_SET);
    lcd_udelay(400);
    HAL_GPIO_WritePin(LCD_EN_PORT, LCD_EN_PIN, GPIO_PIN_RESET);
    lcd_udelay(400);
}

/* Write nibble: bit0 -> D4, bit1 -> D5, bit2 -> D6, bit3 -> D7 */
static void lcd_write_nibble(uint8_t nibble)
{
    nibble &= 0x0F;

    HAL_GPIO_WritePin(LCD_D4_PORT, LCD_D4_PIN, (nibble & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D5_PORT, LCD_D5_PIN, (nibble & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D6_PORT, LCD_D6_PIN, (nibble & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D7_PORT, LCD_D7_PIN, (nibble & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    lcd_udelay(120);
    lcd_pulse_enable();
}

/* Send a byte (isData=1 -> data, 0 -> command) */
static void lcd_send_byte(uint8_t byte, uint8_t isData)
{
    HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, isData ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* send high nibble then low nibble */
    lcd_write_nibble((byte >> 4) & 0x0F);
    lcd_write_nibble(byte & 0x0F);

    lcd_udelay(1200);
}

/* Public API */

void lcd_init(void)
{
    /* Wait for LCD to power up */
    HAL_Delay(50);

    /* ensure control lines are low */
    HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_EN_PORT, LCD_EN_PIN, GPIO_PIN_RESET);

    /* init sequence: 0x03,0x03,0x03,0x02 */
    lcd_write_nibble(0x03);
    HAL_Delay(5);
    lcd_write_nibble(0x03);
    HAL_Delay(5);
    lcd_write_nibble(0x03);
    HAL_Delay(2);
    lcd_write_nibble(0x02);
    HAL_Delay(2);

    /* function set: 4-bit, 2 lines */
    lcd_send_byte(0x28, 0);
    /* display on, cursor off */
    lcd_send_byte(0x0C, 0);
    /* entry mode */
    lcd_send_byte(0x06, 0);
    /* clear */
    lcd_send_byte(0x01, 0);
    HAL_Delay(2);
}

void lcd_clear(void)
{
    lcd_send_byte(0x01, 0);
    HAL_Delay(2);
}

static void lcd_set_line(uint8_t line)
{
    uint8_t addr = 0x00;
    if (line == 1) addr = 0x00;
    else addr = 0x40;
    lcd_send_byte(0x80 | addr, 0);
}

void lcd_print(const char *str)
{
    while (*str) {
        lcd_send_byte((uint8_t)(*str), 1);
        str++;
    }
}

void lcd_print_line(uint8_t line, const char *str)
{
    char buf[17];
    size_t len = strlen(str);
    if (len > 16) len = 16;
    /* pad */
    for (int i = 0; i < 16; ++i) buf[i] = ' ';
    memcpy(buf, str, len);
    buf[16] = '\0';

    lcd_set_line(line);
    HAL_Delay(1);
    for (int i = 0; i < 16; ++i) {
        lcd_send_byte((uint8_t)buf[i], 1);
    }
}
