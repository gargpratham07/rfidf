/*
 ******************************************************************************
 * @file           : state_machine.h
 * @brief          : RFID wallet state machine interface
 * @author         : Group Project — SNU ECE 2025
 * 
 * Defines state machine logic for the cashless RFID wallet system including
 * card detection, menu navigation, balance inquiry, and payment processing.
 ******************************************************************************
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>

/**
 * WalletState: Enumeration of system states
 */
typedef enum {
    STATE_IDLE,                  /**< Waiting for card tap */
    STATE_VENDOR_SELECTED,       /**< Vendor card detected */
    STATE_STUDENT_MENU,          /**< Student menu active */
    STATE_MENU_CYCLE,            /**< Menu option cycling */
    STATE_PAYMENT_CONFIRM,       /**< Confirming payment transaction */
    STATE_SHOW_BALANCE,          /**< Displaying balance */
    STATE_ADD_10,                /**< Adding funds to balance */
    STATE_SUB_1,                 /**< Deducting funds from balance */
    STATE_HISTORY,               /**< Showing transaction history */
    STATE_EXIT_MENU             /**< Exiting menu (return to idle) */
} WalletState;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * state_machine_run
 * @brief Main state machine loop
 * Initializes wallet, RFID reader, LCD, and runs event-driven state machine.
 * Blocks indefinitely (runs main application loop).
 * @param None
 * @return void (never returns)
 */
void state_machine_run(void);

#endif /* STATE_MACHINE_H */
