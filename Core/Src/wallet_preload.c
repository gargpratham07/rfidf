#include "wallet.h"
#include <string.h>
#include <stdint.h>

/*
 * wallet_preload:
 *  - uids: pointer to array [count][5] of 5-byte UIDs
 *  - balances: starting balances (in paise) for each UID
 *  - count: number of entries
 *
 * Behavior:
 *  - For each UID: find existing slot by UID, otherwise create it.
 *  - Add one transaction with the starting balance (credit).
 *  - Add two small demo transactions (so history shows something).
 *
 * Requires the following functions to exist in your wallet implementation:
 *  - int wallet_find_slot_by_uid(const uint8_t uid[5]);
 *  - int wallet_find_or_create_slot(const uint8_t uid[5]);
 *  - void wallet_add_transaction_by_slot(int slot, int32_t amount_paise);
 *
 * (These were referenced elsewhere in your project; if names differ, tell me.)
 */

void wallet_preload(const uint8_t uids[][5], const int32_t balances[], int count)
{
    if (!uids || count <= 0) return;

    if (count > WALLET_MAX_CARDS) count = WALLET_MAX_CARDS;

    for (int i = 0; i < count; ++i)
    {
        const uint8_t *uid = uids[i];
        int slot = wallet_find_slot_by_uid(uid);
        if (slot < 0) {
            slot = wallet_find_or_create_slot(uid);
            if (slot < 0) {
                /* couldn't create slot (wallet full) - skip */
                continue;
            }
        }

        /* Add initial balance as a single 'credit' transaction.
           If you prefer to directly set balance you'll need a wallet API
           that can set balance or clear existing txs — this approach is safe. */
        if (balances) {
            int32_t bal = balances[i];
            if (bal != 0) {
                wallet_add_transaction_by_slot(slot, bal);
            }
        }

        /* Add two small demo transactions so history isn't empty */
        wallet_add_transaction_by_slot(slot, -150); /* small debit */
        wallet_add_transaction_by_slot(slot, -50);  /* small debit */
    }
}
