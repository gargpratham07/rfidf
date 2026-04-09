#ifndef __WALLET_H
#define __WALLET_H

#include <stdint.h>

#define WALLET_MAX_CARDS 4
#define WALLET_MAX_TX    8

typedef struct {
    int32_t amount;    // positive = credit, negative = debit (in paise/cents)
    uint32_t seq;      // simple sequence number for ordering
} WalletTx;

typedef struct {
    uint8_t uid[5];    // 5-byte UID from MFRC522
    int valid;         // 0 = empty slot, 1 = used
    int32_t balance;   // balance in paise/cents
    WalletTx tx[WALLET_MAX_TX];
    uint8_t tx_head;   // index for next write (circular)
    uint8_t tx_count;  // how many txs currently stored (<= WALLET_MAX_TX)
} WalletCard;

void wallet_init(void);
int wallet_find_slot_by_uid(const uint8_t uid[5]);
int wallet_find_or_create_slot(const uint8_t uid[5]);
int32_t wallet_get_balance_by_slot(int slot);
void wallet_add_transaction_by_slot(int slot, int32_t amount);
uint8_t wallet_get_txcount_by_slot(int slot);
int wallet_get_tx_by_slot(int slot, uint8_t idx, WalletTx *out_tx);
int wallet_uid_equal(const uint8_t a[5], const uint8_t b[5]);

#endif
