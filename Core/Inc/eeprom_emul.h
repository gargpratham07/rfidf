#ifndef EEPROM_EMUL_H
#define EEPROM_EMUL_H

#include <stdint.h>
#include "stm32f3xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Tune these if you change wallet limits ---- */
#ifndef MAX_SLOTS
#define MAX_SLOTS 4
#endif

#ifndef MAX_TX_PER_SLOT
#define MAX_TX_PER_SLOT 10
#endif

/* ---- FLASH region used for emulation ----
   DEFAULT is last 2 pages at 0x0807F000 for STM32F303RE (512KB).
   If your MCU has different flash size, change FLASH_USER_START_ADDR and FLASH_USER_PAGE_COUNT.
   Page size assumed 2KB (STM32F3 typical). */
#ifndef FLASH_USER_START_ADDR
#define FLASH_USER_START_ADDR  ((uint32_t)0x0807F000U)
#endif

#ifndef FLASH_USER_PAGE_COUNT
#define FLASH_USER_PAGE_COUNT  2U
#endif

/* API */
int eeprom_emul_init(void);
int eeprom_emul_load_all(void);
int eeprom_emul_save_all(void);
int eeprom_emul_erase_all(void);

/* helper: bytes used */
uint32_t eeprom_emul_storage_size(void);

#ifdef __cplusplus
}
#endif

#endif /* EEPROM_EMUL_H */
