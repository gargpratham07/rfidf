#include "eeprom_emul.h"
#include "wallet.h"
#include <string.h>
#include <stdio.h>

/* Storage layout:
   For each slot:
     uid[5]      (5 bytes)
     txcount     (1 byte)
     txs         (MAX_TX_PER_SLOT * 4 bytes each, int32 little-endian)
   SLOT_SIZE = 5 + 1 + 4*MAX_TX_PER_SLOT
*/

static const uint32_t SLOT_SIZE = (5 + 1 + (4 * MAX_TX_PER_SLOT));
static const uint32_t TOTAL_SIZE = (SLOT_SIZE * MAX_SLOTS);

/* Avoid name clash with headers */
static const uint32_t FLASH_EMU_BASE = FLASH_USER_START_ADDR;

/* pack/unpack int32 le */
static void store_int32_le(uint8_t *p, int32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}
static int32_t load_int32_le(const uint8_t *p) {
    return (int32_t)((int32_t)p[0] | ((int32_t)p[1] << 8) | ((int32_t)p[2] << 16) | ((int32_t)p[3] << 24));
}

uint32_t eeprom_emul_storage_size(void) { return TOTAL_SIZE; }

int eeprom_emul_init(void)
{
    /* Sanity check: ensure TOTAL_SIZE fits in reserved pages */
    /* Page size typically 2KB for many F3 parts; compute area size */
    uint32_t area_bytes = FLASH_USER_PAGE_COUNT * 2048U;
    if (TOTAL_SIZE > area_bytes) {
        printf("EEPROM_EMUL: ERROR storage %lu > area %lu bytes\n", (unsigned long)TOTAL_SIZE, (unsigned long)area_bytes);
        return -2;
    }
    return 0;
}

int eeprom_emul_erase_all(void)
{
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef EraseInit;
    uint32_t PageError = 0;

    EraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInit.PageAddress = FLASH_EMU_BASE;
    EraseInit.NbPages = FLASH_USER_PAGE_COUNT;

    if (HAL_FLASHEx_Erase(&EraseInit, &PageError) != HAL_OK) {
        HAL_FLASH_Lock();
        printf("EEPROM_EMUL: erase fail, PageError=0x%08lX\n", (unsigned long)PageError);
        return -1;
    }

    HAL_FLASH_Lock();
    printf("EEPROM_EMUL: erased %u pages at 0x%08lX\n", (unsigned)FLASH_USER_PAGE_COUNT, (unsigned long)FLASH_EMU_BASE);
    return 0;
}

int eeprom_emul_save_all(void)
{
    uint8_t buf[SLOT_SIZE];
    uint32_t addr = FLASH_EMU_BASE;

    /* Erase target pages first */
    if (eeprom_emul_erase_all() != 0) {
        return -1;
    }

    HAL_FLASH_Unlock();

    for (uint32_t s = 0; s < MAX_SLOTS; ++s) {
        memset(buf, 0xFF, SLOT_SIZE);

        /* --- UID --- */
#ifdef HAVE_WALLET_GET_UID_BY_SLOT
        uint8_t uid[5];
        if (wallet_get_uid_by_slot(s, uid) == 0) {
            memcpy(buf, uid, 5);
        }
#else
        /* If wallet doesn't expose uid-by-slot, we attempt to find uid by reading first txs,
           but safest is to ensure wallet_find_slot_by_uid/preset mapping is used.
           Here we leave UID 0xFF if wallet doesn't expose UID by slot. */
#endif

        /* --- txcount --- */
        uint8_t txc = (uint8_t)wallet_get_txcount_by_slot(s);
        if (txc > MAX_TX_PER_SLOT) txc = MAX_TX_PER_SLOT;
        buf[5] = txc;

        /* --- txs --- */
        for (uint8_t t = 0; t < txc; ++t) {
            WalletTx wt;
            if (wallet_get_tx_by_slot(s, t, &wt) == 0) {
                store_int32_le(&buf[6 + (t * 4)], wt.amount);
            } else {
                store_int32_le(&buf[6 + (t * 4)], 0);
            }
        }

        /* Program as half-words (2 bytes) */
        for (uint32_t off = 0; off < SLOT_SIZE; off += 2) {
            uint16_t half = 0xFFFF;
            if ((off + 1) < SLOT_SIZE) half = (uint16_t)(buf[off] | (buf[off+1] << 8));
            else half = (uint16_t)(buf[off] | (0xFF << 8));

            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + off, (uint32_t)half) != HAL_OK) {
                HAL_FLASH_Lock();
                printf("EEPROM_EMUL: flash write fail at 0x%08lX\n", (unsigned long)(addr+off));
                return -2;
            }
        }

        addr += SLOT_SIZE;
    }

    HAL_FLASH_Lock();
    printf("EEPROM_EMUL: saved %lu bytes at 0x%08lX\n", (unsigned long)TOTAL_SIZE, (unsigned long)FLASH_EMU_BASE);
    return 0;
}

int eeprom_emul_load_all(void)
{
    uint8_t buf[SLOT_SIZE];
    uint32_t addr = FLASH_EMU_BASE;
    uint8_t any_loaded = 0;

    for (uint32_t s = 0; s < MAX_SLOTS; ++s) {
        memcpy(buf, (void*)(addr), SLOT_SIZE);

        /* check UID presence */
        int empty = 1;
        for (int i = 0; i < 5; ++i) if (buf[i] != 0xFF) { empty = 0; break; }
        if (empty) { addr += SLOT_SIZE; continue; }

        uint8_t uid[5];
        memcpy(uid, buf, 5);

        int slot = wallet_find_slot_by_uid(uid);
        if (slot < 0) {
            slot = wallet_find_or_create_slot(uid);
            if (slot < 0) {
                printf("EEPROM_EMUL: cannot create slot for UID %02X %02X %02X %02X %02X\n",
                       uid[0], uid[1], uid[2], uid[3], uid[4]);
                addr += SLOT_SIZE;
                continue;
            }
        }

#ifdef HAVE_WALLET_CLEAR_SLOT
        wallet_clear_slot(slot);
#endif

        uint8_t txc = buf[5];
        if (txc > MAX_TX_PER_SLOT) txc = MAX_TX_PER_SLOT;

        for (uint8_t t = 0; t < txc; ++t) {
            int32_t amount = load_int32_le(&buf[6 + (t * 4)]);
            wallet_add_transaction_by_slot(slot, amount);
        }

        printf("EEPROM_EMUL: loaded slot %d UID %02X %02X %02X %02X %02X (%d txs)\n",
               slot, uid[0], uid[1], uid[2], uid[3], uid[4], txc);
        any_loaded = 1;
        addr += SLOT_SIZE;
    }

    return any_loaded ? 0 : -1;
}
