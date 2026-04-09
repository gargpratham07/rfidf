/*
 ******************************************************************************
 * @file           : state_machine.c
 * @brief          : RFID wallet state machine implementation
 * @author         : Group Project — SNU ECE 2025
 *
 * Implements the main application state machine handling card detection,
 * menu navigation, balance display, and transaction confirmation. Coordinates
 * LCD display output, buzzer feedback, and wallet management. All timing
 * constants and system resources are configured via config.h.
 ******************************************************************************
 */

#include "main.h"
#include "config.h"
#include "stm32f3xx_hal.h"
#include "lcd.h"
#include "mfrc522.h"
#include "wallet.h"
#include "state_machine.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/* ============================================================================
 * User Data & Configuration
 * ============================================================================ */

/** Student UIDand name mapping (preloaded roster) */
typedef struct {
    uint8_t uid[5];
    char name[16];
} UserInfo;

/** Preloaded student roster */
static const UserInfo users[STUDENT_PRELOAD_COUNT] = {
    { {0x47,0x5A,0x95,0xB2,0x3A}, "ROHAN"  },
    { {0xF7,0x88,0x60,0xB2,0xAD}, "ROHIT"  },
    { {0xB7,0xC8,0x64,0xB2,0xA9}, "SNEHA"  },
    { {0x67,0x5B,0x93,0xB2,0x1D}, "DAKSH"  }
};

/** Preset student card UIDs for preload */
static const uint8_t preset_uids[STUDENT_PRELOAD_COUNT][5] = {
    {0x47, 0x5A, 0x95, 0xB2, 0x3A},
    {0xF7, 0x88, 0x60, 0xB2, 0xAD},
    {0xB7, 0xC8, 0x64, 0xB2, 0xA9},
    {0x67, 0x5B, 0x93, 0xB2, 0x1D}
};

/** Preset student starting balances (in paise/cents) */
static const int32_t preset_balances[STUDENT_PRELOAD_COUNT] = { 
    50,     /* ROHAN: 0.50 rupees */
    2000,   /* ROHIT: 20.00 rupees */
    750,    /* SNEHA: 7.50 rupees */
    10000   /* DAKSH: 100.00 rupees */
};

/** Menu operation type enumeration */
typedef enum {
    MENU_BALANCE = 0,
    MENU_ADD_10,
    MENU_SUB_1,
    MENU_HISTORY,
    MENU_EXIT,
    MENU_COUNT
} MenuOption;

/** Menu display strings (one per option) */
static const char *menu_text[MENU_COUNT] = {
    "Show Balance",
    "Add 10.00",
    "Sub 1.00",
    "Show History",
    "Exit"
};

/* ============================================================================
 * State Machine Variables
 * ============================================================================ */

/** Current application state */
static WalletState current_state = STATE_IDLE;

/** Flag: menu active */
static int in_menu = 0;

/** Wallet slot of current card in menu */
static int current_slot = -1;

/** Current menu option index (0 to MENU_COUNT-1) */
static int menu_index = 0;

/** Timer: start of card hold for selection */
static uint32_t present_start_tick = 0;

/** Timer: last menu activity (for inactivity timeout) */
static uint32_t menu_last_activity = 0;

/** Flag: welcome message already shown for this card session */
static uint8_t welcome_shown = 0;

/** Card detection counters (debouncing) */
static int ok = 0;  /**< OK detection count */
static int no = 0;  /**< Missed detection count */

/** Previous and current card presence states */
static int prev_card_present = 0;
static int card_present = 0;

/* ============================================================================
 * Helper Functions - Buzzer Control
 * ============================================================================ */

/**
 * buzzer_beep
 * @brief Simple buzzer tone via GPIO pulse
 * @param ms: Duration in milliseconds
 * @return void
 */
static void buzzer_beep(uint32_t ms)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
    HAL_Delay(ms);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
}

/**
 * high_beep
 * @brief High-frequency buzzer approximation (rapid toggle)
 * Used for urgent alerts (low balance)
 * @param ms_total: Total duration in milliseconds
 * @return void
 */
static void high_beep(uint32_t ms_total)
{
    uint32_t t_start = HAL_GetTick();
    while ((HAL_GetTick() - t_start) < ms_total) {
        HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
        for (volatile int i = 0; i < BUZZER_HIGH_BEEP_LOOP_CYCLES; ++i) {
            __asm__("nop");
        }
        HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
        for (volatile int i = 0; i < BUZZER_HIGH_BEEP_LOOP_CYCLES; ++i) {
            __asm__("nop");
        }
    }
}

/**
 * beep_ok
 * @brief Short success beep
 * @param None
 * @return void
 */
static void beep_ok(void)
{
    buzzer_beep(BUZZER_OK_DURATION_MS);
}

/**
 * beep_done
 * @brief Medium success beep (operation complete)
 * @param None
 * @return void
 */
static void beep_done(void)
{
    buzzer_beep(BUZZER_DONE_DURATION_MS);
}

/**
 * beep_low_alert
 * @brief Urgent alert beep (low balance warning)
 * @param None
 * @return void
 */
static void beep_low_alert(void)
{
    high_beep(BUZZER_LOW_ALERT_DURATION_MS);
}

/* ============================================================================
 * Helper Functions - User Identification
 * ============================================================================ */

/**
 * get_username_str
 * @brief Lookup student name by RFID UID
 * Performs robust byte-by-byte comparison (no assumptions about alignment)
 * @param uid: 5-byte RFID UID
 * @return Pointer to static name string, or "UNKNOWN" if not in roster
 */
static const char* get_username_str(const uint8_t uid[5])
{
    for (int i = 0; i < STUDENT_PRELOAD_COUNT; ++i) {
        uint8_t same = 1;
        for (int b = 0; b < 5; ++b) {
            if (users[i].uid[b] != uid[b]) {
                same = 0;
                break;
            }
        }
        if (same) {
            return users[i].name;
        }
    }
    return "UNKNOWN";
}

/* ============================================================================
 * Helper Functions - Debug Output
 * ============================================================================ */

/**
 * print_uid_uart
 * @brief Print card UID to UART debug console
 * @param uid: 5-byte UID to print
 * @return void
 */
static void print_uid_uart(const uint8_t uid[5])
{
    printf("UID: %02X %02X %02X %02X %02X\r\n", uid[0], uid[1], uid[2], uid[3], uid[4]);
}

/**
 * print_slot_info_uart
 * @brief Print wallet slot balance and transaction history to UART
 * @param slot: Wallet slot index (0-3)
 * @return void
 */
static void print_slot_info_uart(int slot)
{
    if (slot < 0) {
        printf("Slot invalid\r\n");
        return;
    }
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

/* ============================================================================
 * Helper Functions - UI Display
 * ============================================================================ */

/**
 * show_ready_screen
 * @brief Display idle/ready state on LCD
 * @param None
 * @return void
 */
static void show_ready_screen(void)
{
    lcd_clear();
    lcd_print_line(1, "RFID Wallet");
    lcd_print_line(2, "Ready - Tap Card");
}

/**
 * show_welcome
 * @brief Display welcome message with username
 * @param uid: Card UID (for username lookup)
 * @return void
 */
static void show_welcome(const uint8_t uid[5])
{
    lcd_clear();
    lcd_print_line(1, "WELCOME");
    lcd_print_line(2, get_username_str(uid));
    HAL_Delay(WELCOME_DISPLAY_MS);
    welcome_shown = 1;
}

/**
 * show_menu_option
 * @brief Display current menu option on LCD
 * @param uid: Card UID (for username formatting)
 * @return void
 */
static void show_menu_option(const uint8_t uid[5])
{
    char l1[LCD_LINE_MAX_CHARS];
    char l2[LCD_LINE_MAX_CHARS];
    snprintf(l1, sizeof(l1), "%s's Wallet", get_username_str(uid));
    snprintf(l2, sizeof(l2), "%s", menu_text[menu_index]);
    lcd_clear();
    lcd_print_line(1, l1);
    lcd_print_line(2, l2);
}

/**
 * preload_wallet_local
 * @brief Initialize wallet with preloaded student cards and demo transactions
 * @param uids: Array of UIDs to preload
 * @param balances: Array of initial balances (in paise)
 * @param count: Number of entries
 * @return void
 */
static void preload_wallet_local(const uint8_t uids[][5], const int32_t balances[], int count)
{
    if (!uids || count <= 0) {
        return;
    }
    
    if (count > WALLET_MAX_CARDS) {
        count = WALLET_MAX_CARDS;
    }
    
    for (int i = 0; i < count; ++i) {
        const uint8_t *uid = uids[i];
        int slot = wallet_find_slot_by_uid(uid);
        
        if (slot < 0) {
            slot = wallet_find_or_create_slot(uid);
            if (slot < 0) {
                printf("Preload: cannot create slot for UID ");
                for (int k = 0; k < 5; ++k) {
                    printf("%02X ", uid[k]);
                }
                printf("\r\n");
                continue;
            }
        }
        
        /* Add initial balance transaction */
        if (balances && balances[i] != 0) {
            wallet_add_transaction_by_slot(slot, balances[i]);
        }
        
        /* Add demo transactions for history display */
        wallet_add_transaction_by_slot(slot, -DEMO_TX_DEBIT_1_PAISE);
        wallet_add_transaction_by_slot(slot, -DEMO_TX_DEBIT_2_PAISE);
        
        printf("Preloaded slot %d for UID %02X %02X %02X %02X %02X\r\n",
               slot, uid[0], uid[1], uid[2], uid[3], uid[4]);
    }
}

/**
 * wait_card_removed
 * @brief Block until card is removed (debounced)
 * @param uid: UID buffer for polling
 * @return void
 */
static void wait_card_removed(uint8_t uid[5])
{
    while (card_present) {
        HAL_Delay(CARD_WAIT_LOOP_INTERVAL_MS);
        uint8_t st = MFRC522_Check(uid);
        if (st == MI_OK) {
            ok = CARD_STABLE_COUNT;
            no = 0;
            card_present = 1;
        } else {
            no++;
            ok = 0;
            card_present = (ok >= CARD_STABLE_COUNT);
        }
    }
}

/**
 * handle_unknown_card
 * @brief Process event when unknown/unregistered card is detected
 * @param uid: Unknown card UID
 * @return void
 */
static void handle_unknown_card(uint8_t uid[5])
{
    printf("UNREGISTERED CARD\r\n");
    lcd_clear();
    lcd_print_line(1, "UNREGISTERED");
    lcd_print_line(2, get_username_str(uid));
    buzzer_beep(BUZZER_OK_DURATION_MS);
    HAL_Delay(CARD_WAIT_LOOP_INTERVAL_MS);
    buzzer_beep(BUZZER_OK_DURATION_MS);
    
    wait_card_removed(uid);
    welcome_shown = 0;
    show_ready_screen();
}

/**
 * perform_menu_action
 * @brief Execute the selected menu action (balance, topup, etc.)
 * Updates state and wallet, displays result on LCD
 * @param uid: Current card UID (for display)
 * @return void
 */
static void perform_menu_action(const uint8_t uid[5])
{
    const char *label = get_username_str(uid);
    printf("Selecting option %d (%s) for label %s slot %d\r\n",
           menu_index, menu_text[menu_index], label, current_slot);

    if (menu_index == MENU_BALANCE) {
        current_state = STATE_SHOW_BALANCE;
        int32_t bal = wallet_get_balance_by_slot(current_slot);
        char buf[LCD_LINE_MAX_CHARS];
        uint32_t absbal = (uint32_t)((bal < 0) ? -bal : bal);
        snprintf(buf, sizeof(buf), "Bal %lu.%02lu",
                 (unsigned long)(absbal/100), (unsigned long)(absbal%100));
        
        if (bal < LOW_BALANCE_THRESHOLD_PAISE) {
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
        current_state = STATE_ADD_10;
        wallet_add_transaction_by_slot(current_slot, MENU_TOPUP_AMOUNT_PAISE);
        lcd_clear();
        lcd_print_line(1, "Added 10.00");
        lcd_print_line(2, "Success");
        print_slot_info_uart(current_slot);
        beep_done();
        
    } else if (menu_index == MENU_SUB_1) {
        current_state = STATE_SUB_1;
        wallet_add_transaction_by_slot(current_slot, -MENU_DEBIT_AMOUNT_PAISE);
        int32_t bal = wallet_get_balance_by_slot(current_slot);
        char buf[LCD_LINE_MAX_CHARS];
        uint32_t absbal = (uint32_t)((bal < 0) ? -bal : bal);
        snprintf(buf, sizeof(buf), "%lu.%02lu",
                 (unsigned long)(absbal/100), (unsigned long)(absbal%100));
        
        if (bal < LOW_BALANCE_THRESHOLD_PAISE) {
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
        current_state = STATE_HISTORY;
        uint8_t cnt = wallet_get_txcount_by_slot(current_slot);
        printf("History (count=%d):\r\n", cnt);
        lcd_clear();
        lcd_print_line(1, "History:");
        
        for (uint8_t i = 0; i < cnt && i < MENU_HISTORY_DISPLAY_LINES; i++) {
            WalletTx t;
            if (wallet_get_tx_by_slot(current_slot, i, &t) == 0) {
                char b[LCD_LINE_MAX_CHARS];
                snprintf(b, sizeof(b), "%ld.%02ld",
                         (long)(t.amount/100), (long)llabs(t.amount%100));
                lcd_print_line(2, b);
                printf(" #%lu: %ld.%02ld\r\n", (unsigned long)t.seq,
                       (long)(t.amount/100), (long)llabs(t.amount%100));
                HAL_Delay(900);
            }
        }
        beep_done();
        
    } else if (menu_index == MENU_EXIT) {
        current_state = STATE_EXIT_MENU;
        lcd_clear();
        lcd_print_line(1, "Exiting Menu");
        lcd_print_line(2, "Remove Card");
        beep_ok();
        in_menu = 0;
    }
}
    snprintf(l1, sizeof(l1), "%s's Wallet", get_username_str(uid));
    snprintf(l2, sizeof(l2), "%s", menu_text[menu_index]);
    lcd_clear();
    lcd_print_line(1, l1);
    lcd_print_line(2, l2);
}

static void preload_wallet_local(const uint8_t uids[][5], const int32_t balances[], int count) {
    if (!uids || count <= 0) return;
    if (count > WALLET_MAX_CARDS) count = WALLET_MAX_CARDS;
    for (int i = 0; i < count; ++i) {
        const uint8_t *uid = uids[i];
        int slot = wallet_find_slot_by_uid(uid);
        if (slot < 0) {
            slot = wallet_find_or_create_slot(uid);
            if (slot < 0) {
                printf("Preload: cannot create slot for UID ");
                for (int k = 0; k < 5; ++k) printf("%02X ", uid[k]);
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

static void wait_card_removed(uint8_t uid[5]) {
    while (card_present) {
        HAL_Delay(80);
        uint8_t st = MFRC522_Check(uid);
        if (st == MI_OK) { ok = STABLE_COUNT; no = 0; card_present = 1; }
        else { no++; ok = 0; card_present = 0; }
    }
}

static void handle_unknown_card(uint8_t uid[5]) {
    printf("UNREGISTERED CARD\r\n");
    lcd_clear();
    lcd_print_line(1, "UNREGISTERED");
    lcd_print_line(2, get_username_str(uid));
    buzzer_beep(60);
    HAL_Delay(80);
    buzzer_beep(60);
    wait_card_removed(uid);
    welcome_shown = 0;
    show_ready_screen();
}

static void perform_menu_action(const uint8_t uid[5]) {
    const char *label = get_username_str(uid);
    printf("Selecting option %d (%s) for label %s slot %d\r\n", menu_index, menu_text[menu_index], label, current_slot);

    if (menu_index == MENU_BALANCE) {
        current_state = STATE_SHOW_BALANCE;
        int32_t bal = wallet_get_balance_by_slot(current_slot);
        char buf[17];
        uint32_t absbal = (uint32_t)((bal < 0) ? -bal : bal);
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
        current_state = STATE_ADD_10;
        wallet_add_transaction_by_slot(current_slot, 1000);
        lcd_clear();
        lcd_print_line(1, "Added 10.00");
        lcd_print_line(2, "Success");
        print_slot_info_uart(current_slot);
        beep_done();
    } else if (menu_index == MENU_SUB_1) {
        current_state = STATE_SUB_1;
        wallet_add_transaction_by_slot(current_slot, -100);
        int32_t bal = wallet_get_balance_by_slot(current_slot);
        char buf[17];
        uint32_t absbal = (uint32_t)((bal < 0) ? -bal : bal);
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
        current_state = STATE_HISTORY;
        uint8_t cnt = wallet_get_txcount_by_slot(current_slot);
        printf("History (count=%d):\r\n", cnt);
        lcd_clear();
        lcd_print_line(1, "History:");
        for (uint8_t i = 0; i < cnt && i < 4; i++) {
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
        current_state = STATE_EXIT_MENU;
        lcd_clear();
        lcd_print_line(1, "Exiting Menu");
        lcd_print_line(2, "Remove Card");
        beep_ok();
        in_menu = 0;
    }
}

/**
 * state_machine_run
 * @brief Main RFID wallet application state machine loop
 *
 * Implements the core transaction flow:
 * 1. Card Detection: Debounces MFRC522 card presence to detect new cards
 * 2. Menu Navigation: Cycles through menu options (balance, add credit, subtract, history, exit)
 * 3. Hold-to-Select: Waits for MENU_HOLD_TIME_MS card presence to confirm menu action
 * 4. Inactivity Timeout: Exits menu after MENU_INACTIVITY_TIMEOUT_MS without activity
 * 5. Exit Button: Supports EXIT_BUTTON (PC13) to manually escape menu
 *
 * Peripheral initialization sequence: wallet_init → MFRC522 reset and init → lcd_init
 * Student roster preloaded via preload_wallet_local() at startup.
 *
 * @return void (infinite loop, never returns)
 */
void state_machine_run(void) {
    uint8_t uid[5];
    uint8_t st;

    /* ========================================================================
     * System Initialization
     * ======================================================================== */
    
    /* Initialize wallet data structures and preload student roster */
    wallet_init();
    preload_wallet_local(preset_uids, preset_balances, STUDENT_PRELOAD_COUNT);

    /* MFRC522 hardware reset: hold RST low then high to force reset */
    HAL_GPIO_WritePin(MFRC522_RST_GPIO_Port, MFRC522_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(MFRC522_INIT_DELAY_MS);
    HAL_GPIO_WritePin(MFRC522_RST_GPIO_Port, MFRC522_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(MFRC522_INIT_DELAY_MS);
    
    /* Configure MFRC522 registers (SPI, modulation, antenna, etc.) */
    MFRC522_Init();
    HAL_Delay(MFRC522_INIT_DELAY_MS);

    /* Initialize LCD display (4-bit mode, clear screen, backlight on) */
    lcd_init();
    HAL_Delay(MFRC522_INIT_DELAY_MS);
    lcd_clear();
    lcd_print_line(1, "RFID Wallet");
    lcd_print_line(2, "Tap Card to Start");
    HAL_Delay(WELCOME_DISPLAY_MS);
    lcd_clear();

    printf("RFID Wallet started.\r\n");
    show_ready_screen();

    /* ========================================================================
     * Main Event Loop
     * ======================================================================== */
    while (1) {
        /* Check EXIT button (PC13, active low) */
        uint8_t exit_button_pressed = (HAL_GPIO_ReadPin(EXIT_BUTTON_GPIO_Port, EXIT_BUTTON_Pin) == GPIO_PIN_RESET);

        /* Card Detection Polling
         * MFRC522_Check() sends PICC_REQIDL to detect card presence.
         * Debounce by counting consecutive detections (ok counter).
         */
        st = MFRC522_Check(uid);
        if (st == MI_OK) { ok++; no = 0; }
        else { no++; ok = 0; }

        /* Edge detection: transition from no-card → card-present */
        prev_card_present = card_present;
        card_present = (ok >= CARD_STABLE_COUNT);
        int new_appearance = (!prev_card_present && card_present);

        /* ====================================================================
         * Exit Button Handler (manual menu exit during menu mode)
         * ==================================================================== */
        if (in_menu && exit_button_pressed) {
            in_menu = 0;
            welcome_shown = 0;
            lcd_clear();
            lcd_print_line(1, "Menu exited");
            lcd_print_line(2, "By button");
            printf("Menu exited by button.\r\n");
            HAL_Delay(600);
            show_ready_screen();
            wait_card_removed(uid);
            continue;
        }

        /* ====================================================================
         * Card Appearance Handler (new card detected)
         * ==================================================================== */
        if (new_appearance) {
            const char *username = get_username_str(uid);
            print_uid_uart(uid);
            beep_ok();

            /* Lookup student in wallet database */
            int slot = wallet_find_slot_by_uid(uid);
            if (!welcome_shown) {
                show_welcome(uid);
            }

            /* If card not in roster, show error and eject menu */
            if (slot < 0) {
                handle_unknown_card(uid);
                continue;
            }

            current_state = STATE_VENDOR_SELECTED;

            /* Menu state transitions: first tap enters menu, subsequent taps cycle options */
            if (!in_menu) {
                /* First card tap in idle state: enter menu at option 0 (balance) */
                in_menu = 1;
                menu_index = 0;
                current_slot = slot;
                menu_last_activity = HAL_GetTick();
                present_start_tick = HAL_GetTick();
            } else {
                /* Card tapped again while in menu: cycle to next option (with wraparound) */
                menu_index = (menu_index + 1) % MENU_COUNT;
                menu_last_activity = HAL_GetTick();
                present_start_tick = HAL_GetTick();
            }

            current_state = STATE_STUDENT_MENU;
            show_menu_option(uid);

            /* Immediate low-balance warning when cycles to or lands on BALANCE option */
            if (menu_index == MENU_BALANCE) {
                int32_t bal = wallet_get_balance_by_slot(current_slot);
                if (bal < LOW_BALANCE_THRESHOLD_PAISE) {
                    char b[LCD_LINE_MAX_CHARS];
                    uint32_t absbal = (uint32_t)((bal < 0) ? -bal : bal);
                    snprintf(b, sizeof(b), "%lu.%02lu", (unsigned long)(absbal/100), (unsigned long)(absbal%100));
                    lcd_clear();
                    lcd_print_line(1, "LOW BALANCE");
                    lcd_print_line(2, b);
                    printf("LOW BALANCE slot %d: %ld.%02ld\r\n", current_slot, (long)(bal/100), (long)llabs(bal%100));
                    beep_low_alert();
                    HAL_Delay(800);
                    show_menu_option(uid);
                }
            }
        }

        /* ====================================================================
         * Hold-to-Select Handler (card held on reader for MENU_HOLD_TIME_MS)
         * ==================================================================== */
        if (in_menu && card_present) {
            if (present_start_tick == 0) present_start_tick = HAL_GetTick();
            uint32_t held = HAL_GetTick() - present_start_tick;
            menu_last_activity = HAL_GetTick();

            /* If card held long enough, execute selected menu action */
            if (held >= MENU_HOLD_TIME_MS) {
                current_state = STATE_PAYMENT_CONFIRM;
                perform_menu_action(uid);
                wait_card_removed(uid);
                welcome_shown = 0;
                present_start_tick = 0;

                /* Return to menu or idle depending on in_menu flag set by perform_menu_action() */
                if (in_menu) {
                    show_menu_option(uid);
                } else {
                    show_ready_screen();
                }
            }
        }

        /* Card removed during hold: reset hold timer */
        if (in_menu && !card_present) {
            present_start_tick = 0;
        }

        /* ====================================================================
         * Inactivity Timeout Handler (exit menu if no activity for timeout period)
         * ==================================================================== */
        if (in_menu && (HAL_GetTick() - menu_last_activity) > MENU_INACTIVITY_TIMEOUT_MS) {
            in_menu = 0;
            welcome_shown = 0;
            current_state = STATE_IDLE;
            lcd_clear();
            lcd_print_line(1, "Menu Timeout");
            lcd_print_line(2, "Ready - Tap Card");
            printf("Menu timeout.\r\n");
            HAL_Delay(800);
            show_ready_screen();
        }

        /* Poll loop interval */
        HAL_Delay(CARD_WAIT_LOOP_INTERVAL_MS);
    }
}
