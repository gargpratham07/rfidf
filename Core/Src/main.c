/*
 ******************************************************************************
 * @file           : main.c
 * @brief          : System initialization and state machine entry point
 * @author         : Group Project — SNU ECE 2025
 *
 * Initializes the STM32F303RE microcontroller including system clock, GPIO
 * peripherals, SPI1 (RFID reader), and UART2 (debug console). After all
 * hardware is configured, transfers control to state_machine_run() which
 * implements the RFID wallet application logic. This file contains only
 * initialization code; all business logic resides in Core/Src/state_machine.c
 * and supporting driver modules.
 ******************************************************************************
 */

#include "main.h"
#include "config.h"
#include "state_machine.h"

/** SPI1 handle for MFRC522 communication */
SPI_HandleTypeDef hspi1;

/** UART2 handle for debug output */
UART_HandleTypeDef huart2;

/* Function prototypes */
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
void SystemClock_Config(void);
void Error_Handler(void);

/* ============================================================================
 * Debug Console Redirection
 * ============================================================================ */

/**
 * __io_putchar
 * @brief Printf() redirection to UART2 for debug output
 * Allows use of printf() throughout application. Each character is transmitted
 * via HAL_UART_Transmit() to the debug console (115200 baud, 8N1).
 * @param ch: Character code to output
 * @return ch (return value for compatibility with putchar signature)
 */
int __io_putchar(int ch) {
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart2, &c, 1, HAL_MAX_DELAY);
    return ch;
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

/**
 * main
 * @brief System initialization and application entry point
 * 
 * Initialization sequence:
 * 1. HAL_Init() - Initialize STM32 HAL library
 * 2. SystemClock_Config() - Configure HSI PLL to 72 MHz
 * 3. MX_GPIO_Init() - Configure GPIO for LCD, RFID, buzzer, buttons
 * 4. MX_USART2_UART_Init() - Configure UART2 at 115200 baud for debug
 * 5. MX_SPI1_Init() - Configure SPI1 for RFID reader communication
 * 6. state_machine_run() - Transfer control to RFID wallet state machine
 *
 * @return 0 (never reached, state_machine_run() is infinite loop)
 */
int main(void)
{
    /* Initialize STM32 HAL (SysTick timer, NVIC, etc.) */
    HAL_Init();
    
    /* Configure system clock: HSI (8 MHz) × 9 = 72 MHz */
    SystemClock_Config();

    /* Initialize GPIO pins for all peripherals */
    MX_GPIO_Init();
    
    /* Initialize UART2 for debug console */
    MX_USART2_UART_Init();
    
    /* Initialize SPI1 for RFID reader */
    MX_SPI1_Init();

    /* Transfer control to RFID wallet state machine (never returns) */
    state_machine_run();

    return 0;
}

/* ============================================================================
 * Peripheral Initialization Functions
 * ============================================================================ */

/**
 * MX_SPI1_Init
 * @brief Configure SPI1 for MFRC522 RFID reader communication
 *
 * SPI1 protocol configuration:
 * - Mode: Master (STM32F303RE controls the clock)
 * - Data Direction: Full duplex (2-line MOSI/MISO)
 * - Data Size: 8-bit (MFRC522 register and FIFO access)
 * - Clock Polarity: Low (CPOL=0, idle level is LOW)
 * - Clock Phase: 1-edge (CPHA=0, data sampled on leading edge)
 * - NSS: Software controlled (manual CS management via PA4 GPIO)
 * - Clock Prescaler: 64 (72 MHz / 64 = 1.125 MHz for MFRC522)
 * - Bit Order: MSB-first (MFRC522 expects MSB transmitted first)
 *
 * GPIO pins configured separately in MX_GPIO_Init():
 * - PA5: SCK (SPI1 clock output)
 * - PA6: MISO (SPI1 data input from MFRC522)
 * - PA7: MOSI (SPI1 data output to MFRC522)
 * - PA4: CS (manual control for chip select, not hardware)
 *
 * @return void
 * @note Calls Error_Handler() if HAL_SPI_Init() fails
 */
static void MX_SPI1_Init(void) {
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
}

/**
 * MX_USART2_UART_Init
 * @brief Configure UART2 for debug console at 115200 baud
 *
 * UART2 configuration (debug output):
 * - Baud Rate: 115200 (standard for serial terminals)
 * - Data Bits: 8 (8 bits per character)
 * - Stop Bits: 1 (one stop bit, standard)
 * - Parity: None (no parity bit)
 * - Flow Control: None (no hardware RTS/CTS, no software Xon/Xoff)
 * - Mode: TX+RX (transmit and receive)
 * - Oversampling: 16x (standard STM32 UART oversampling)
 *
 * GPIO pins configured separately in MX_GPIO_Init():
 * - PA9: USART2_TX (debug output)
 * - PA10: USART2_RX (debug input, not used for this application)
 *
 * Usage: Integrated with __io_putchar() for printf() redirection
 *
 * @return void
 * @note Calls Error_Handler() if HAL_UART_Init() fails
 */
static void MX_USART2_UART_Init(void) {
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

/**
 * MX_GPIO_Init
 * @brief Configure all GPIO pins for LCD, RFID, buzzer, and buttons
 *
 * GPIO Port Organization:
 * 
 * GPIOA (Buzzer & SPI):
 *   PA4:  CS (MFRC522 chip select, active low)
 *   PA5:  SCK (SPI1 clock, alternate function)
 *   PA6:  MISO (SPI1 receive, alternate function)
 *   PA7:  MOSI (SPI1 transmit, alternate function)
 *   PA8:  Buzzer (active high PWM output)
 *   PA9:  USART2_TX (debug output, alternate function)
 *   PA10: USART2_RX (debug input, alternate function)
 *
 * GPIOB (LCD control & data & RFID reset):
 *   PB1:  LCD RS (register select, push-pull output)
 *   PB2:  LCD EN (strobe/enable, push-pull output)
 *   PB10: LCD D4 (data bit 4, push-pull output)
 *   PB11: LCD D5 (data bit 5, push-pull output)
 *   PB12: LCD D6 (data bit 6, push-pull output) + MFRC522 RST
 *   PB13: LCD D7 (data bit 7, push-pull output)
 *
 * GPIOC (Button input):
 *   PC13: EXIT button (input with pull-up, active low)
 *   PC14-PC15: Reserved as inputs (pull-up)
 *
 * GPIOF (Reserved):
 *   No pins used, clock enabled for completeness
 *
 * Port Clock Enabling:
 *   All GPIO port clocks enabled to ensure pin accessibility. This is safe
 *   and has minimal power impact. Unused ports can be disabled in production.
 *
 * @return void
 * @note No error handling in GPIO init (GPIO_Init() rarely fails at runtime)
 */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Enable GPIO port clocks for all used peripherals */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* ========================================================================
   * LCD Data & Control Pins (GPIOB)
   * ======================================================================== */
  
  /* LCD RS (PB1) and EN (PB2) control lines: push-pull outputs (initially low) */
  HAL_GPIO_WritePin(GPIOB, LCD_RS_Pin|LCD_EN_Pin, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = LCD_RS_Pin|LCD_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* LCD D4-D7 (PB10-PB13) data lines: push-pull outputs (initially low) */
  HAL_GPIO_WritePin(GPIOB, LCD_D4_Pin|LCD_D5_Pin|LCD_D6_Pin|LCD_D7_Pin, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = LCD_D4_Pin|LCD_D5_Pin|LCD_D6_Pin|LCD_D7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* ========================================================================
   * MFRC522 Hardware Reset (GPIOB)
   * ======================================================================== */
  
  /* PB12 RST (reset pin, active high, default to HIGH for normal operation) */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* ========================================================================
   * Buzzer Output (GPIOA)
   * ======================================================================== */
  
  /* PA8 Buzzer (active high PWM, default LOW for silent state) */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* ========================================================================
   * Button Input (GPIOC)
   * ======================================================================== */
  
  /* PC13 EXIT button (input with pull-up, active low) */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* PC14-PC15 Reserved buttons (input with pull-up for safety) */
  GPIO_InitStruct.Pin = GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* ========================================================================
   * SPI1 Bus Pins (GPIOA)
   * ======================================================================== */
  
  /* PA4 CS (chip select, manual control, start HIGH for no selection) */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PA5/PA6/PA7 SPI1 bus (SCK, MISO, MOSI as alternate function) */
  GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/* ============================================================================
 * System Clock Configuration
 * ============================================================================ */

/**
 * SystemClock_Config
 * @brief Configure system clock to 72 MHz using HSI PLL
 *
 * Clock tree configuration:
 * - Input: HSI (High Speed Internal oscillator) at 8 MHz
 * - PLL Multiplier: 9 (8 MHz × 9 = 72 MHz)
 * - System Clock (SYSCLK): 72 MHz (from PLL output)
 * - AHB Clock (HCLK): 72 MHz (no divider, AHB prescaler = 1)
 * - APB1 Clock (PCLK1): 36 MHz (APB1 prescaler = 2) [max 36 MHz on F303]
 * - APB2 Clock (PCLK2): 72 MHz (APB2 prescaler = 1) [SPI clock domain]
 * - UART2 Clock: PCLK1 = 36 MHz (USART2 on APB1 bus)
 * - Flash Latency: 1 wait state (required for 72 MHz operation)
 *
 * This clock configuration gives:
 * - SPI1 clock (APB2): Up to 72 MHz (actual SPI freq 72MHz / prescaler)
 * - UART2 clock: 36 MHz (actual baud rate 36MHz / divider for 115200)
 * - Peripheral timers: Various clocks on APB1 and APB2
 *
 * @return void
 * @note Calls Error_Handler() if any clock configuration step fails
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /* ========================================================================
   * Oscillator Configuration (HSI + PLL)
   * ======================================================================== */
  
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;  /* Use HSI (8 MHz) as PLL input */
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;          /* Multiply by 9: 8 MHz × 9 = 72 MHz */
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;       /* No predivider on F303 */
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  /* ========================================================================
   * Clock Distribution (SYSCLK, AHB, APB1, APB2)
   * ======================================================================== */
  
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;  /* Use PLL as SYSCLK source */
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;         /* HCLK = SYSCLK / 1 = 72 MHz */
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;          /* PCLK1 = HCLK / 2 = 36 MHz */
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;          /* PCLK2 = HCLK / 1 = 72 MHz */
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) Error_Handler();

  /* ========================================================================
   * Peripheral Clock Configuration (UART2)
   * ======================================================================== */
  
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;  /* Use PCLK1 (36 MHz) for UART2 */
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();
}

/* ============================================================================
 * Error Handler
 * ============================================================================ */

/**
 * Error_Handler
 * @brief Fatal error handler - called when initialization fails
 *
 * If any critical system initialization fails (clock config, GPIO, SPI, etc.),
 * this function is called to halt the system. Disables interrupts and enters
 * an infinite loop to indicate system failure.
 *
 * In a production system, this might:
 * - Flash an LED to signal error
 * - Log error code to EEPROM
 * - Trigger watchdog reset
 * - Output error to debug UART before disable
 *
 * @return void (never returns)
 */
void Error_Handler(void) {
  __disable_irq();
  while (1) { }
}
