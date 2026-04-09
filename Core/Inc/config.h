/*
 ******************************************************************************
 * @file           : config.h
 * @brief          : Centralized configuration for RFID Wallet system
 * @author         : Group Project — SNU ECE 2025
 * 
 * This header defines all compile-time constants, GPIO pin mappings, timing
 * parameters, system limits, and threshold values used throughout the project.
 * Modify values here to reconfigure system behavior without editing source files.
 ******************************************************************************
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* ============================================================================
 * Timing Parameters (milliseconds)
 * ============================================================================ */

/** Card detection stability threshold (debounce) */
#define CARD_STABLE_COUNT               2

/** Hold time required on card to select menu option */
#define MENU_HOLD_TIME_MS               800

/** Menu inactivity timeout before auto-exit to idle state */
#define MENU_INACTIVITY_TIMEOUT_MS      30000

/** Brief display delay for welcome message */
#define WELCOME_DISPLAY_MS              700

/** Delay for short buzzer alert */
#define BUZZER_OK_DURATION_MS           60

/** Delay for mid-length buzzer (success/done) */
#define BUZZER_DONE_DURATION_MS         120

/** Duration for low-balance alert buzzer */
#define BUZZER_LOW_ALERT_DURATION_MS    500

/** Card removal wait loop interval */
#define CARD_WAIT_LOOP_INTERVAL_MS      80

/** Display hold time after menu action before returning to menu */
#define POST_ACTION_DISPLAY_HOLD_MS     800

/** Initial power-up delay for LCD stabilization */
#define LCD_POWERUP_DELAY_MS            50

/** Delay after each LCD command transmission */
#define LCD_GENERIC_DELAY_MS            2

/** Delay after operations like clear or init */
#define LCD_LONG_DELAY_MS               50

/** MFRC522 reset/init sequence delay */
#define MFRC522_RESET_DELAY_MS          50

/** MFRC522 init stabilization delay */
#define MFRC522_INIT_DELAY_MS           50

/* ============================================================================
 * Micro-delay (cycle-based) Parameters (for LCD nibble operations)
 * ============================================================================ */

/** Cycles for short micro-delay (nibble timing) */
#define LCD_UDELAY_SHORT_CYCLES         120

/** Cycles for enable pulse duration */
#define LCD_ENABLE_PULSE_CYCLES         400

/** Cycles for longer operation delays (command timing) */
#define LCD_UDELAY_MED_CYCLES           1200

/** Cycles for high-beep frequency simulation */
#define BUZZER_HIGH_BEEP_LOOP_CYCLES    300

/* ============================================================================
 * Wallet & Transaction System
 * ============================================================================ */

/** Maximum number of concurrent student/vendor cards in wallet */
#define WALLET_MAX_CARDS                4

/** Maximum transaction history entries per card */
#define WALLET_MAX_TRANSACTIONS         8

/** Number of preloaded student cards */
#define STUDENT_PRELOAD_COUNT           4

/** Currency: amount in paise/cents (100 = 1.00 in rupees) */
#define LOW_BALANCE_THRESHOLD_PAISE     100

/** Topup amount for menu "Add 10.00" option (in paise) */
#define MENU_TOPUP_AMOUNT_PAISE         1000

/** Debit amount for menu "Sub 1.00" option (in paise) */
#define MENU_DEBIT_AMOUNT_PAISE         100

/** Demo transaction debit for preload (in paise) */
#define DEMO_TX_DEBIT_1_PAISE           150

/** Demo transaction debit for preload (in paise) */
#define DEMO_TX_DEBIT_2_PAISE           50

/* ============================================================================
 * Menu System
 * ============================================================================ */

/** Maximum menu options per state */
#define MENU_OPTIONS_COUNT              5

/** Maximum visible history entries on LCD (16x2 constraint) */
#define MENU_HISTORY_DISPLAY_LINES      4

/** Maximum characters per LCD line */
#define LCD_LINE_MAX_CHARS              17

/* ============================================================================
 * LCD Pin Definitions (4-bit HD44780 mode)
 * ============================================================================ */

/** LCD Register Select (RS) - command/data mode */
#define LCD_RS_GPIO_Port                GPIOB
#define LCD_RS_Pin                      GPIO_PIN_1

/** LCD Enable (EN/E) - data latch pulse */
#define LCD_EN_GPIO_Port                GPIOB
#define LCD_EN_Pin                      GPIO_PIN_2

/** LCD Data bit 0 (DB4) */
#define LCD_D4_GPIO_Port                GPIOB
#define LCD_D4_Pin                      GPIO_PIN_10

/** LCD Data bit 1 (DB5) */
#define LCD_D5_GPIO_Port                GPIOB
#define LCD_D5_Pin                      GPIO_PIN_11

/** LCD Data bit 2 (DB6) */
#define LCD_D6_GPIO_Port                GPIOB
#define LCD_D6_Pin                      GPIO_PIN_12

/** LCD Data bit 3 (DB7) */
#define LCD_D7_GPIO_Port                GPIOB
#define LCD_D7_Pin                      GPIO_PIN_13

/* ============================================================================
 * MFRC522 RFID Reader Pins (SPI1)
 * ============================================================================ */

/** MFRC522 Chip Select (CS) - active low */
#define MFRC522_CS_GPIO_Port            GPIOA
#define MFRC522_CS_Pin                  GPIO_PIN_4

/** MFRC522 Serial Clock (SCK) - SPI1_SCK */
#define MFRC522_SCK_GPIO_Port           GPIOA
#define MFRC522_SCK_Pin                 GPIO_PIN_5

/** MFRC522 Master In Slave Out (MISO) - SPI1_MISO */
#define MFRC522_MISO_GPIO_Port          GPIOA
#define MFRC522_MISO_Pin                GPIO_PIN_6

/** MFRC522 Master Out Slave In (MOSI) - SPI1_MOSI */
#define MFRC522_MOSI_GPIO_Port          GPIOA
#define MFRC522_MOSI_Pin                GPIO_PIN_7

/** MFRC522 Reset (RST) - active high, active low reset */
#define MFRC522_RST_GPIO_Port           GPIOB
#define MFRC522_RST_Pin                 GPIO_PIN_12

/* ============================================================================
 * Control & Feedback Pins
 * ============================================================================ */

/** Buzzer output - PWM simulated via GPIO toggle */
#define BUZZER_GPIO_Port                GPIOA
#define BUZZER_Pin                      GPIO_PIN_8

/** Exit button (menu exit) - active low */
#define EXIT_BUTTON_GPIO_Port           GPIOC
#define EXIT_BUTTON_Pin                 GPIO_PIN_13

/** Confirm button (reserved for future use) - active low */
#define CONFIRM_BUTTON_GPIO_Port        GPIOC
#define CONFIRM_BUTTON_Pin              GPIO_PIN_14

/* ============================================================================
 * UART Configuration (Serial Debug Output)
 * ============================================================================ */

/** UART baud rate for debug console output */
#define UART_BAUD_RATE                 115200

/* ============================================================================
 * SPI Configuration (MFRC522 Communication)
 * ============================================================================ */

/** SPI baud rate prescaler for MFRC522 (64 = ~700 kHz on 72 MHz clock) */
#define SPI_BAUDRATE_PRESCALER          SPI_BAUDRATEPRESCALER_64

#endif /* CONFIG_H */
