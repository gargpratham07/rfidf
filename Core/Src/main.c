/* Core/Src/main.c
   Fixed: show name only on first detection, show "<NAME>'s Wallet" instead of Slot N,
   robust UID->name mapping, improved menu behavior.
   Replace existing main.c (backup first).
*/

#include "main.h"
#include "lcd.h"
#include "mfrc522.h"
#include "wallet.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h> /* llabs */

#define STABLE_COUNT 2
#define HOLD_TIME_MS 800
#define MENU_INACTIVITY_TIMEOUT_MS 30000
#define PRELOAD_COUNT 4
#define LOW_BALANCE_THRESHOLD 100 /* paise = 1.00 */

/* ---------- Users mapping ---------- */
typedef struct {
    uint8_t uid[5];
    char name[16];
} UserInfo;

static UserInfo users[PRELOAD_COUNT] = {
    { {0x47,0x5A,0x95,0xB2,0x3A}, "ROHAN" },
    { {0xF7,0x88,0x60,0xB2,0xAD}, "ROHIT" },
    { {0xB7,0xC8,0x64,0xB2,0xA9}, "SNEHA" },
    { {0x67,0x5B,0x93,0xB2,0x1D}, "DAKSH" }
};

static const uint8_t preset_uids[PRELOAD_COUNT][5] = {
    {0x47, 0x5A, 0x95, 0xB2, 0x3A},
    {0xF7, 0x88, 0x60, 0xB2, 0xAD},
    {0xB7, 0xC8, 0x64, 0xB2, 0xA9},
    {0x67, 0x5B, 0x93, 0xB2, 0x1D}
};

static const int32_t preset_balances[PRELOAD_COUNT] = { 50, 2000, 750, 10000 };

/* ---------- Menu ---------- */
typedef enum { MENU_BALANCE = 0, MENU_ADD_10, MENU_SUB_1, MENU_HISTORY, MENU_EXIT, MENU_COUNT } MenuOption;
static const char *menu_text[MENU_COUNT] = { "Show Balance", "Add 10.00", "Sub 1.00", "Show History", "Exit" };

/* ---------- HW ---------- */
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart2;

/* Forward prototypes */
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
void SystemClock_Config(void);
void Error_Handler(void);

/* ---------- Helpers ---------- */
int __io_putchar(int ch) {
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart2, &c, 1, HAL_MAX_DELAY);
    return ch;
}

/* basic beep (blocking) */
static void buzzer_beep(uint32_t ms) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_Delay(ms);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
}

/* high/urgent beep approximation */
static void high_beep(uint32_t ms_total) {
    uint32_t t_start = HAL_GetTick();
    while ((HAL_GetTick() - t_start) < ms_total) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
        for (volatile int i = 0; i < 300; ++i) __asm__("nop");
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
        for (volatile int i = 0; i < 300; ++i) __asm__("nop");
    }
}

static void beep_ok(void)   { buzzer_beep(60); }
static void beep_done(void) { buzzer_beep(120); }
static void beep_low_alert(void) { high_beep(500); }

static void print_uid_uart(const uint8_t uid[5]) {
    printf("UID: %02X %02X %02X %02X %02X\r\n", uid[0], uid[1], uid[2], uid[3], uid[4]);
}

static void print_slot_info_uart(int slot) {
    if (slot < 0) { printf("Slot invalid\r\n"); return; }
    int32_t bal = wallet_get_balance_by_slot(slot);
    printf("Slot %d balance: %ld.%02ld\r\n", slot, (long)(bal/100), (long)llabs(bal%100));
    uint8_t cnt = wallet_get_txcount_by_slot(slot);
    printf("Tx count: %d\r\n", cnt);
    for (uint8_t i = 0; i < cnt; ++i) {
        WalletTx t;
        if (wallet_get_tx_by_slot(slot, i, &t) == 0) {
            printf("  #%lu: %ld.%02ld\r\n", (unsigned long)t.seq,
                   (long)(t.amount/100), (long)llabs(t.amount%100));
        }
    }
}

/* robust UID->name lookup: compares 5 bytes explicitly */
static const char* get_username_str(const uint8_t uid[5]) {
    for (int i = 0; i < PRELOAD_COUNT; ++i) {
        uint8_t same = 1;
        for (int b = 0; b < 5; ++b) {
            if (users[i].uid[b] != uid[b]) { same = 0; break; }
        }
        if (same) return users[i].name;
    }
    return "UNKNOWN";
}

/* local preload helper */
static void wallet_preload_local(const uint8_t uids[][5], const int32_t balances[], int count) {
    if (!uids || count <= 0) return;
    if (count > WALLET_MAX_CARDS) count = WALLET_MAX_CARDS;
    for (int i = 0; i < count; ++i) {
        const uint8_t *uid = uids[i];
        int slot = wallet_find_slot_by_uid(uid);
        if (slot < 0) {
            slot = wallet_find_or_create_slot(uid);
            if (slot < 0) {
                printf("Preload: cannot create slot for UID ");
                for (int k=0;k<5;k++) printf("%02X ", uid[k]);
                printf("\r\n");
                continue;
            }
        }
        if (balances && balances[i] != 0) wallet_add_transaction_by_slot(slot, balances[i]);
        wallet_add_transaction_by_slot(slot, -150);
        wallet_add_transaction_by_slot(slot, -50);
        printf("Preloaded slot %d for UID %02X %02X %02X %02X %02X\r\n",
               slot, uid[0], uid[1], uid[2], uid[3], uid[4]);
    }
}

/* ---------- MAIN ---------- */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_SPI1_Init();

    wallet_init();
    wallet_preload_local(preset_uids, preset_balances, PRELOAD_COUNT);

    /* reset MFRC522 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
    HAL_Delay(50);
    MFRC522_Init();
    HAL_Delay(50);

    lcd_init();
    HAL_Delay(50);
    lcd_clear();
    lcd_print_line(1, "RFID Wallet");
    lcd_print_line(2, "Tap Card to Start");
    HAL_Delay(700);
    lcd_clear();

    printf("RFID Wallet (names fixed) started.\r\n");

    uint8_t uid[5];
    int ok = 0, no = 0;
    int prev_card_present = 0;
    int card_present = 0;

    int in_menu = 0;
    int current_slot = -1;
    int menu_index = 0;
    uint32_t present_start_tick = 0;
    uint32_t menu_last_activity = 0;

    /* track whether we've shown welcome already for the current card session */
    uint8_t welcome_shown = 0;

    while (1) {
        /* exit button (PC13) active low */
        uint8_t exit_button_pressed = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET);

        uint8_t st = MFRC522_Check(uid);
        if (st == MI_OK) { ok++; no = 0; }
        else { no++; ok = 0; }

        prev_card_present = card_present;
        card_present = (ok >= STABLE_COUNT);

        int new_appearance = (!prev_card_present && card_present);

        /* Exit button when in menu: immediate exit */
        if (in_menu && exit_button_pressed) {
            in_menu = 0;
            welcome_shown = 0;
            lcd_clear();
            lcd_print_line(1, "Menu exited");
            lcd_print_line(2, "By button");
            printf("Menu exited by button.\r\n");
            HAL_Delay(600);
            lcd_clear();
            lcd_print_line(1, "RFID Wallet");
            lcd_print_line(2, "Ready - Tap Card");
            /* wait until card removed */
            while (card_present) {
                HAL_Delay(80);
                st = MFRC522_Check(uid);
                if (st == MI_OK) { ok = STABLE_COUNT; no = 0; card_present = 1; }
                else { no++; ok = 0; card_present = 0; }
            }
            continue;
        }

        /* Fresh appearance handling */
        if (new_appearance) {
            const char *username = get_username_str(uid);
            print_uid_uart(uid);
            beep_ok();

            int slot = wallet_find_slot_by_uid(uid);

            /* Show welcome only once per card-present session */
            if (!welcome_shown) {
                lcd_clear();
                lcd_print_line(1, "WELCOME");
                lcd_print_line(2, username);
                HAL_Delay(700);
                welcome_shown = 1;
            }

            if (slot < 0) {
                /* unknown card */
                printf("UNREGISTERED CARD\r\n");
                lcd_clear();
                lcd_print_line(1, "UNREGISTERED");
                lcd_print_line(2, username);
                buzzer_beep(60); HAL_Delay(80); buzzer_beep(60);

                /* wait for removal */
                while (card_present) {
                    HAL_Delay(80);
                    st = MFRC522_Check(uid);
                    if (st == MI_OK) { ok = STABLE_COUNT; no = 0; card_present = 1; }
                    else { no++; ok = 0; card_present = 0; }
                }
                welcome_shown = 0;
                lcd_clear();
                lcd_print_line(1, "RFID Wallet");
                lcd_print_line(2, "Ready - Tap Card");
                continue;
            }

            /* Enter menu or cycle */
            if (!in_menu) {
                in_menu = 1;
                menu_index = 0;
                current_slot = slot;
                menu_last_activity = HAL_GetTick();
                present_start_tick = HAL_GetTick();
            } else {
                /* cycle option on fresh tap */
                menu_index = (menu_index + 1) % MENU_COUNT;
                menu_last_activity = HAL_GetTick();
                present_start_tick = HAL_GetTick();
            }

            /* show menu text with friendly wallet label instead of "Slot N" */
            char l1[17]; char l2[17];
            const char *label = get_username_str(uid);
            snprintf(l1, sizeof(l1), "%s's Wallet", label);
            snprintf(l2, sizeof(l2), "%s", menu_text[menu_index]);
            lcd_clear();
            lcd_print_line(1, l1);
            lcd_print_line(2, l2);

            /* immediate low-balance warning when balance option visible */
            if (menu_index == MENU_BALANCE) {
                int32_t bal = wallet_get_balance_by_slot(current_slot);
                if (bal < LOW_BALANCE_THRESHOLD) {
                    char b[17];
                    uint32_t absbal = (uint32_t)((bal<0)?-bal:bal);
                    snprintf(b, sizeof(b), "%lu.%02lu", (unsigned long)(absbal/100), (unsigned long)(absbal%100));
                    lcd_clear();
                    lcd_print_line(1, "LOW BALANCE");
                    lcd_print_line(2, b);
                    printf("LOW BALANCE slot %d: %ld.%02ld\r\n", current_slot, (long)(bal/100), (long)llabs(bal%100));
                    beep_low_alert();
                    HAL_Delay(800);
                    /* return to menu display */
                    snprintf(l1, sizeof(l1), "%s's Wallet", label);
                    snprintf(l2, sizeof(l2), "%s", menu_text[menu_index]);
                    lcd_clear();
                    lcd_print_line(1, l1);
                    lcd_print_line(2, l2);
                }
            }
        }

        /* While card present & in menu -> check hold for selection */
        if (in_menu && card_present) {
            if (present_start_tick == 0) present_start_tick = HAL_GetTick();
            uint32_t held = HAL_GetTick() - present_start_tick;
            menu_last_activity = HAL_GetTick();

            if (held >= HOLD_TIME_MS) {
                const char *label = get_username_str(uid);
                printf("Selecting option %d (%s) for label %s slot %d\r\n", menu_index, menu_text[menu_index], label, current_slot);

                if (menu_index == MENU_BALANCE) {
                    int32_t bal = wallet_get_balance_by_slot(current_slot);
                    char buf[17];
                    uint32_t absbal = (uint32_t)((bal<0)?-bal:bal);
                    snprintf(buf, sizeof(buf), "Bal %lu.%02lu", (unsigned long)(absbal/100), (unsigned long)(absbal%100));
                    if (bal < LOW_BALANCE_THRESHOLD) {
                        lcd_clear();
                        lcd_print_line(1, "LOW BALANCE");
                        lcd_print_line(2, buf);
                        beep_low_alert();
                    } else {
                        lcd_clear();
                        lcd_print_line(1, "Balance");
                        lcd_print_line(2, buf);
                        beep_done();
                    }
                    print_slot_info_uart(current_slot);
                } else if (menu_index == MENU_ADD_10) {
                    wallet_add_transaction_by_slot(current_slot, 1000);
                    lcd_clear();
                    lcd_print_line(1, "Added 10.00");
                    lcd_print_line(2, "Success");
                    print_slot_info_uart(current_slot);
                    beep_done();
                } else if (menu_index == MENU_SUB_1) {
                    wallet_add_transaction_by_slot(current_slot, -100);
                    int32_t bal = wallet_get_balance_by_slot(current_slot);
                    char buf[17];
                    uint32_t absbal = (uint32_t)((bal<0)?-bal:bal);
                    snprintf(buf, sizeof(buf), "%lu.%02lu", (unsigned long)(absbal/100), (unsigned long)(absbal%100));
                    if (bal < LOW_BALANCE_THRESHOLD) {
                        lcd_clear();
                        lcd_print_line(1, "LOW BALANCE");
                        lcd_print_line(2, buf);
                        beep_low_alert();
                    } else {
                        lcd_clear();
                        lcd_print_line(1, "Subtracted 1.00");
                        lcd_print_line(2, "Done");
                        beep_done();
                    }
                    print_slot_info_uart(current_slot);
                } else if (menu_index == MENU_HISTORY) {
                    uint8_t cnt = wallet_get_txcount_by_slot(current_slot);
                    printf("History (count=%d):\r\n", cnt);
                    lcd_clear();
                    lcd_print_line(1, "History:");
                    for (uint8_t i=0;i<cnt && i<4;i++) {
                        WalletTx t;
                        if (wallet_get_tx_by_slot(current_slot, i, &t) == 0) {
                            char b[17];
                            snprintf(b, sizeof(b), "%ld.%02ld", (long)(t.amount/100), (long)llabs(t.amount%100));
                            lcd_print_line(2, b);
                            printf(" #%lu: %ld.%02ld\r\n", (unsigned long)t.seq, (long)(t.amount/100), (long)llabs(t.amount%100));
                            HAL_Delay(900);
                        }
                    }
                    beep_done();
                } else if (menu_index == MENU_EXIT) {
                    lcd_clear();
                    lcd_print_line(1, "Exiting Menu");
                    lcd_print_line(2, "Remove Card");
                    beep_ok();
                    in_menu = 0;
                }

                /* wait until card removed to avoid retrigger */
                while (card_present) {
                    HAL_Delay(80);
                    st = MFRC522_Check(uid);
                    if (st == MI_OK) { ok = STABLE_COUNT; no = 0; card_present = 1; }
                    else { no++; ok = 0; card_present = 0; }
                }
                /* reset welcome flag so next new card shows welcome again */
                welcome_shown = 0;
                present_start_tick = 0;

                if (in_menu) {
                    char l1[17], l2[17];
                    snprintf(l1, sizeof(l1), "%s's Wallet", get_username_str(uid));
                    snprintf(l2, sizeof(l2), "%s", menu_text[menu_index]);
                    lcd_clear();
                    lcd_print_line(1, l1);
                    lcd_print_line(2, l2);
                } else {
                    lcd_clear();
                    lcd_print_line(1, "RFID Wallet");
                    lcd_print_line(2, "Ready - Tap Card");
                }
            }
        }

        /* card removed without selection -> stay in menu (user can tap to cycle) */
        if (in_menu && !card_present) {
            present_start_tick = 0;
        }

        /* menu inactivity timeout */
        if (in_menu && (HAL_GetTick() - menu_last_activity) > MENU_INACTIVITY_TIMEOUT_MS) {
            in_menu = 0;
            welcome_shown = 0;
            lcd_clear();
            lcd_print_line(1, "Menu Timeout");
            lcd_print_line(2, "Ready - Tap Card");
            printf("Menu timeout.\r\n");
            HAL_Delay(800);
            lcd_clear();
            lcd_print_line(1, "RFID Wallet");
            lcd_print_line(2, "Ready - Tap Card");
        }

        HAL_Delay(80);
    }

    return 0;
}

/* ---------------- Peripheral initializers (replace with CubeMX versions if present) ---------------- */

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

static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* LCD PB0..PB5 outputs */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* PB12 MFRC522 RST */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Buzzer PA8 */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Exit button PC13 input pull-up (active low) */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* Keep PC14/15 as input pull-up if present */
  GPIO_InitStruct.Pin = GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* CS (PA4) default HIGH */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* SPI1 pins PA5/PA6/PA7 AF */
  GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) Error_Handler();

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();
}

void Error_Handler(void) {
  __disable_irq();
  while (1) { }
}
