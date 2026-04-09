/*
 ******************************************************************************
 * @file           : wallet.c
 * @brief          : Wallet and transaction management implementation
 * @author         : Group Project — SNU ECE 2025
 * 
 * Implements in-RAM wallet storage with per-card balance tracking and
 * circular transaction history. Provides bounds-checked access to prevent
 * buffer overflows and data corruption.
 ******************************************************************************
 */

#include "wallet.h"
#include <string.h>

/* ============================================================================
 * Private Module State
 * ============================================================================ */

/** Array of all wallet card slots in system memory */
static WalletCard cards[WALLET_MAX_CARDS];

/** Global transaction sequence counter (monotonically increasing) */
static uint32_t global_seq = 1;

/* ============================================================================
 * Wallet API Implementation
 * ============================================================================ */

void wallet_init(void)
{
    /* Clear all card slots and reset sequence */
    memset(cards, 0, sizeof(cards));
    global_seq = 1;
}

int wallet_uid_equal(const uint8_t a[5], const uint8_t b[5])
{
    /* Compare two 5-byte UIDs using memcmp for safety */
    return (memcmp(a, b, 5) == 0);
}

int wallet_find_slot_by_uid(const uint8_t uid[5])
{
    /* Search all wallet slots for matching UID */
    for (int i = 0; i < WALLET_MAX_CARDS; ++i) {
        if (cards[i].valid && wallet_uid_equal(cards[i].uid, uid)) {
            return i;
        }
    }
    return -1;
}

int wallet_find_or_create_slot(const uint8_t uid[5])
{
    /* First try to find existing slot */
    int slot = wallet_find_slot_by_uid(uid);
    if (slot >= 0) {
        return slot;
    }

    /* Search for empty slot and initialize */
    for (int i = 0; i < WALLET_MAX_CARDS; ++i) {
        if (!cards[i].valid) {
            cards[i].valid = 1;
            memcpy(cards[i].uid, uid, 5);
            cards[i].balance = 0;
            cards[i].tx_head = 0;
            cards[i].tx_count = 0;
            return i;
        }
    }
    
    /* No slots available */
    return -1;
}

int32_t wallet_get_balance_by_slot(int slot)
{
    /* Bounds check: return 0 if slot invalid or empty */
    if (slot < 0 || slot >= WALLET_MAX_CARDS) {
        return 0;
    }
    if (!cards[slot].valid) {
        return 0;
    }
    return cards[slot].balance;
}

void wallet_add_transaction_by_slot(int slot, int32_t amount)
{
    /* Bounds check: silently ignore if slot invalid */
    if (slot < 0 || slot >= WALLET_MAX_CARDS) {
        return;
    }
    if (!cards[slot].valid) {
        return;
    }

    /* Create new transaction entry */
    WalletTx t;
    t.amount = amount;
    t.seq = global_seq++;

    /* Insert into circular buffer at head, advance head */
    cards[slot].tx[cards[slot].tx_head] = t;
    cards[slot].tx_head = (cards[slot].tx_head + 1) % WALLET_MAX_TRANSACTIONS;

    /* Increment transaction count if buffer not yet full */
    if (cards[slot].tx_count < WALLET_MAX_TRANSACTIONS) {
        cards[slot].tx_count++;
    }

    /* Update card balance */
    cards[slot].balance += amount;
}

uint8_t wallet_get_txcount_by_slot(int slot)
{
    /* Bounds check: return 0 if slot invalid or empty */
    if (slot < 0 || slot >= WALLET_MAX_CARDS) {
        return 0;
    }
    if (!cards[slot].valid) {
        return 0;
    }
    return cards[slot].tx_count;
}

int wallet_get_tx_by_slot(int slot, uint8_t idx, WalletTx *out_tx)
{
    /* Validate parameters */
    if (!out_tx) {
        return -1;
    }
    if (slot < 0 || slot >= WALLET_MAX_CARDS) {
        return -1;
    }
    if (!cards[slot].valid) {
        return -1;
    }
    if (idx >= cards[slot].tx_count) {
        return -1;
    }

    /* Calculate position in circular buffer: most recent = idx 0 */
    int end = (int)cards[slot].tx_head - 1;
    if (end < 0) {
        end += WALLET_MAX_TRANSACTIONS;
    }

    int pos = end - (int)idx;
    while (pos < 0) {
        pos += WALLET_MAX_TRANSACTIONS;
    }
    pos %= WALLET_MAX_TRANSACTIONS;

    /* Copy transaction to output */
    *out_tx = cards[slot].tx[pos];
    return 0;
}

