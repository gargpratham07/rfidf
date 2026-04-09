#include "wallet.h"
#include <string.h>

static WalletCard cards[WALLET_MAX_CARDS];
static uint32_t global_seq = 1;

void wallet_init(void)
{
    memset(cards, 0, sizeof(cards));
    global_seq = 1;
}

int wallet_uid_equal(const uint8_t a[5], const uint8_t b[5])
{
    return (memcmp(a, b, 5) == 0);
}

int wallet_find_slot_by_uid(const uint8_t uid[5])
{
    for (int i = 0; i < WALLET_MAX_CARDS; ++i) {
        if (cards[i].valid && wallet_uid_equal(cards[i].uid, uid)) return i;
    }
    return -1;
}

int wallet_find_or_create_slot(const uint8_t uid[5])
{
    int slot = wallet_find_slot_by_uid(uid);
    if (slot >= 0) return slot;

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
    return -1;
}

int32_t wallet_get_balance_by_slot(int slot)
{
    if (slot < 0 || slot >= WALLET_MAX_CARDS) return 0;
    if (!cards[slot].valid) return 0;
    return cards[slot].balance;
}

void wallet_add_transaction_by_slot(int slot, int32_t amount)
{
    if (slot < 0 || slot >= WALLET_MAX_CARDS) return;
    if (!cards[slot].valid) return;

    WalletTx t;
    t.amount = amount;
    t.seq = global_seq++;

    cards[slot].tx[cards[slot].tx_head] = t;
    cards[slot].tx_head = (cards[slot].tx_head + 1) % WALLET_MAX_TX;

    if (cards[slot].tx_count < WALLET_MAX_TX) cards[slot].tx_count++;

    cards[slot].balance += amount;
}

uint8_t wallet_get_txcount_by_slot(int slot)
{
    if (slot < 0 || slot >= WALLET_MAX_CARDS) return 0;
    if (!cards[slot].valid) return 0;
    return cards[slot].tx_count;
}

int wallet_get_tx_by_slot(int slot, uint8_t idx, WalletTx *out_tx)
{
    if (!out_tx) return -1;
    if (slot < 0 || slot >= WALLET_MAX_CARDS) return -1;
    if (!cards[slot].valid) return -1;
    if (idx >= cards[slot].tx_count) return -1;

    int end = (int)cards[slot].tx_head - 1;
    if (end < 0) end += WALLET_MAX_TX;

    int pos = end - (int)idx;
    while (pos < 0) pos += WALLET_MAX_TX;
    pos %= WALLET_MAX_TX;

    *out_tx = cards[slot].tx[pos];
    return 0;
}
