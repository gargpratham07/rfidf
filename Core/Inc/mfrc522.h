/*
 ******************************************************************************
 * @file           : mfrc522.h
 * @brief          : MFRC522 RFID reader driver interface (SPI mode)
 * @author         : Group Project — SNU ECE 2025
 * 
 * Provides API for communicating with MFRC522 ISO14443A RFID reader module
 * via SPI1. Supports card detection, authentication, UID reading, and card
 * selection. Pin definitions are centralized in config.h.
 ******************************************************************************
 */

#ifndef MFRC522_H
#define MFRC522_H

#include "stm32f3xx_hal.h"
#include "config.h"

/* ============================================================================
 * GPIO Pin Definitions (using config.h)
 * ============================================================================ */

#define MFRC522_CS_PORT             MFRC522_CS_GPIO_Port
#define MFRC522_CS_PIN              MFRC522_CS_Pin

#define MFRC522_RST_PORT            MFRC522_RST_GPIO_Port
#define MFRC522_RST_PIN             MFRC522_RST_Pin

/* ============================================================================
 * MFRC522 Command Values (PCD = Proximity Coupling Device)
 * ============================================================================ */

#define PCD_IDLE                    0x00    /**< No action / Idle mode */
#define PCD_AUTHENT                 0x0E    /**< Authenticate using key */
#define PCD_RECEIVE                 0x08    /**< Receive data */
#define PCD_TRANSMIT                0x04    /**< Transmit data */
#define PCD_TRANSCEIVE              0x0C    /**< Transmit & receive (together) */
#define PCD_RESETPHASE              0x0F    /**< Software reset */
#define PCD_CALCCRC                 0x03    /**< Calculate CRC */

/* ============================================================================
 * PICC Commands (PICC = Proximity Integrated Circuit Card)
 * ============================================================================ */

#define PICC_REQIDL                 0x26    /**< Request (Idle state) */
#define PICC_REQALL                 0x52    /**< Request (All commands) */
#define PICC_ANTICOLL               0x93    /**< Anti-collision (Cascade Level 1) */
#define PICC_SELECT_TAG             0x93    /**< Select tag */
#define PICC_AUTHENT1A              0x60    /**< Authenticate using Key A */
#define PICC_AUTHENT1B              0x61    /**< Authenticate using Key B */
#define PICC_READ                   0x30    /**< Read block */
#define PICC_WRITE                  0xA0    /**< Write block */
#define PICC_DECREMENT              0xC0    /**< Decrement (value block) */
#define PICC_INCREMENT              0xC1    /**< Increment (value block) */
#define PICC_RESTORE                0xC2    /**< Restore (value block) */
#define PICC_TRANSFER               0xB0    /**< Transfer (value block) */
#define PICC_HALT                   0x50    /**< Halt card (low power mode) */

/* ============================================================================
 * Status Codes / Return Values
 * ============================================================================ */

#define MI_OK                       0       /**< Operation successful */
#define MI_NOTAGERR                 1       /**< No tag detected / data error */
#define MI_ERR                      2       /**< General error */

/* ============================================================================
 * MFRC522 Register Addresses
 * ============================================================================ */

#define CommandReg                  0x01    /**< Command register */
#define CommIEnReg                  0x02    /**< Interrupt enable register */
#define DivIEnReg                   0x03    /**< Divide interrupt enable register */
#define CommIrqReg                  0x04    /**< Interrupt request register */
#define DivIrqReg                   0x05    /**< Divide interrupt request register */
#define ErrorReg                    0x06    /**< Error register */
#define Status1Reg                  0x07    /**< Status 1 register */
#define Status2Reg                  0x08    /**< Status 2 register */
#define FIFODataReg                 0x09    /**< FIFO data register (R/W) */
#define FIFOLevelReg                0x0A    /**< FIFO level register */
#define ControlReg                  0x0C    /**< Control register */
#define BitFramingReg               0x0D    /**< Bit framing register */
#define ModeReg                     0x11    /**< Mode register */
#define TxModeReg                   0x12    /**< Transmit mode register */
#define RxModeReg                   0x13    /**< Receive mode register */
#define TxControlReg                0x14    /**< TX control register */
#define TxASKReg                    0x15    /**< TX ASK register */
#define CRCResultRegL               0x22    /**< CRC result L register (low byte) */
#define CRCResultRegM               0x21    /**< CRC result M register (high byte) */
#define TModeReg                    0x2A    /**< Timer mode register */
#define TPrescalerReg               0x2B    /**< Timer prescaler register */
#define TReloadRegH                 0x2C    /**< Timer reload H register */
#define TReloadRegL                 0x2D    /**< Timer reload L register */

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * MFRC522_Init
 * @brief Initialize MFRC522 module (reset, antenna on, configure)
 * @param None
 * @return void
 */
void MFRC522_Init(void);

/**
 * MFRC522_WriteRegister
 * @brief Write single byte to MFRC522 register via SPI
 * @param reg: Register address
 * @param value: Byte value to write
 * @return void
 */
void MFRC522_WriteRegister(uint8_t reg, uint8_t value);

/**
 * MFRC522_ReadRegister
 * @brief Read single byte from MFRC522 register via SPI
 * @param reg: Register address
 * @return Register value
 */
uint8_t MFRC522_ReadRegister(uint8_t reg);

/**
 * MFRC522_SetBitMask
 * @brief Set specified bits in MFRC522 register (logical OR)
 * @param reg: Register address
 * @param mask: Bitmask with 1s for bits to set
 * @return void
 */
void MFRC522_SetBitMask(uint8_t reg, uint8_t mask);

/**
 * MFRC522_ClearBitMask
 * @brief Clear specified bits in MFRC522 register (logical AND with NOT mask)
 * @param reg: Register address
 * @param mask: Bitmask with 1s for bits to clear
 * @return void
 */
void MFRC522_ClearBitMask(uint8_t reg, uint8_t mask);

/**
 * MFRC522_AntennaOn
 * @brief Enable RFID reader antenna (TX pins)
 * @param None
 * @return void
 */
void MFRC522_AntennaOn(void);

/**
 * MFRC522_AntennaOff
 * @brief Disable RFID reader antenna (TX pins)
 * @param None
 * @return void
 */
void MFRC522_AntennaOff(void);

/**
 * MFRC522_Reset
 * @brief Perform soft reset of MFRC522 module
 * @param None
 * @return void
 */
void MFRC522_Reset(void);

/**
 * MFRC522_Request
 * @brief Send request command to detect card presence
 * @param reqMode: Request mode (PICC_REQIDL or PICC_REQALL)
 * @param TagType: Pointer to receive tag type (2 bytes)
 * @return MI_OK on success, MI_NOTAGERR if no card, MI_ERR on error
 */
uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *TagType);

/**
 * MFRC522_ToCard
 * @brief Transmit command to card and receive response
 * General protocol handler for most card communication
 * @param command: Command to send (PCD_* command constant)
 * @param sendData: Pointer to data to transmit
 * @param sendLen: Length of data to transmit (bytes)
 * @param backData: Pointer to buffer for response
 * @param backLen: Pointer to response length variable (filled on return)
 * @return MI_OK on success, MI_NOTAGERR on timeout, MI_ERR on error
 */
uint8_t MFRC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen,
                       uint8_t *backData, uint16_t *backLen);

/**
 * MFRC522_Anticoll
 * @brief Perform anti-collision and get card UID
 * Must call after MFRC522_Request() succeeds
 * @param serNum: Pointer to receive 5-byte UID
 * @return MI_OK on success, MI_ERR on error
 */
uint8_t MFRC522_Anticoll(uint8_t *serNum);

/**
 * MFRC522_SelectTag
 * @brief Select card and get SAK (Select Acknowledge) response
 * Must call after MFRC522_Anticoll() succeeds
 * @param serNum: Pointer to 5-byte UID
 * @return MI_OK on success, MI_ERR on error
 */
uint8_t MFRC522_SelectTag(uint8_t *serNum);

/**
 * MFRC522_Halt
 * @brief Put card into HALT mode (low power / idle state)
 * Use this to properly deactivate card after transaction
 * @param None
 * @return void
 */
void MFRC522_Halt(void);

#endif /* MFRC522_H */
