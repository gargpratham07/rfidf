/*
 ******************************************************************************
 * @file           : wallet.h
 * @brief          : Wallet and transaction management interface
 * @author         : Group Project — SNU ECE 2025
 * 
 * Provides APIs for managing student/vendor card balances and transaction
 * history in RAM-based wallet storage. Each card has a UID, balance, and
 * circular transaction buffer for recording debits/credits.
 ******************************************************************************
 */

#ifndef WALLET_H
#define WALLET_H

#include <stdint.h>
#include "config.h"

/**
 * WalletTx: Single transaction entry (debit or credit)
 */
typedef struct {
    int32_t amount;     /**< Transaction amount in paise (positive=credit, negative=debit) */
    uint32_t seq;       /**< Sequence number for transaction ordering */
} WalletTx;

/**
 * WalletCard: Wallet slot for one card's balance and history
 */
typedef struct {
    uint8_t uid[5];                      /**< 5-byte RFID card UID */
    int valid;                           /**< Status: 0=empty, 1=active */
    int32_t balance;                     /**< Current balance in paise */
    WalletTx tx[WALLET_MAX_TRANSACTIONS]; /**< Circular transaction history */
    uint8_t tx_head;                     /**< Head pointer for circular buffer */
    uint8_t tx_count;                    /**< Number of stored transactions */
} WalletCard;

/* ============================================================================
 * Wallet API Functions
 * ============================================================================ */

/**
 * wallet_init
 * @brief Initialize wallet system (clear all cards and reset seq counter)
 * @param None
 * @return void
 */
void wallet_init(void);

/**
 * wallet_find_slot_by_uid
 * @brief Find wallet slot index by card UID
 * @param uid: 5-byte RFID UID
 * @return Slot index (0-3) on match, -1 if not found
 */
int wallet_find_slot_by_uid(const uint8_t uid[5]);

/**
 * wallet_find_or_create_slot
 * @brief Find existing slot or create new one for UID
 * @param uid: 5-byte RFID UID
 * @return Slot index (0-3) on success, -1 if wallet full
 */
int wallet_find_or_create_slot(const uint8_t uid[5]);

/**
 * wallet_get_balance_by_slot
 * @brief Get current balance for a wallet slot
 * @param slot: Wallet slot index (0-3)
 * @return Balance in paise, 0 if slot invalid
 */
int32_t wallet_get_balance_by_slot(int slot);

/**
 * wallet_add_transaction_by_slot
 * @brief Record a transaction (debit or credit) for a slot
 * @param slot: Wallet slot index (0-3)
 * @param amount: Transaction amount in paise (positive=credit, negative=debit)
 * @return void (fails silently if slot invalid)
 */
void wallet_add_transaction_by_slot(int slot, int32_t amount);

/**
 * wallet_get_txcount_by_slot
 * @brief Get number of transactions recorded in a slot
 * @param slot: Wallet slot index (0-3)
 * @return Transaction count (0-WALLET_MAX_TRANSACTIONS), 0 if slot invalid
 */
uint8_t wallet_get_txcount_by_slot(int slot);

/**
 * wallet_get_tx_by_slot
 * @brief Retrieve transaction at index idx from slot (most recent = idx 0)
 * @param slot: Wallet slot index (0-3)
 * @param idx: Transaction index (0=most recent, 1=previous, etc.)
 * @param out_tx: Pointer to WalletTx struct to fill
 * @return 0 on success, -1 on error (invalid slot, out of bounds, null pointer)
 */
int wallet_get_tx_by_slot(int slot, uint8_t idx, WalletTx *out_tx);

/**
 * wallet_uid_equal
 * @brief Compare two 5-byte UIDs for equality
 * @param a: First UID
 * @param b: Second UID
 * @return 1 if equal, 0 if different
 */
int wallet_uid_equal(const uint8_t a[5], const uint8_t b[5]);

#endif /* WALLET_H */
