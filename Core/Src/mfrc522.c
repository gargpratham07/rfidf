/*
 ******************************************************************************
 * @file           : mfrc522.c
 * @brief          : MFRC522 RFID reader driver implementation (SPI mode)
 * @author         : Group Project — SNU ECE 2025
 * 
 * Implements SPI-based communication with MFRC522 ISO14443A RFID reader.
 * Supports card detection, anti-collision, UID readout, and card selection.
 * All SPI operations use CS (chip select) GPIO toggle. Pin definitions are
 * in config.h.
 ******************************************************************************
 */

#include "mfrc522.h"
#include "config.h"
#include "stm32f3xx_hal.h"
#include <string.h>

/* ============================================================================
 * Module State & External Dependencies
 * ============================================================================ */

/** SPI handle declared in main.c and initialized by CubeMX */
extern SPI_HandleTypeDef hspi1;

/* ============================================================================
 * Private Helper Functions
 * ============================================================================ */

/**
 * MFRC522_Select
 * @brief Assert CS (Chip Select) low to select MFRC522
 * @param None
 * @return void (inline)
 */
static inline void MFRC522_Select(void)
{
    HAL_GPIO_WritePin(MFRC522_CS_PORT, MFRC522_CS_PIN, GPIO_PIN_RESET);
}

/**
 * MFRC522_Unselect
 * @brief De-assert CS (Chip Select) high to deselect MFRC522
 * @param None
 * @return void (inline)
 */
static inline void MFRC522_Unselect(void)
{
    HAL_GPIO_WritePin(MFRC522_CS_PORT, MFRC522_CS_PIN, GPIO_PIN_SET);
}

/**
 * MFRC522_CalculateCRC
 * @brief Calculate CRC checksum using MFRC522 hardware CRC engine
 * Writes data to FIFO, triggers CRC calculation, reads result
 * @param data: Input buffer to calculate CRC for
 * @param len: Length of input data (bytes)
 * @param result: Output buffer for 2-byte CRC result
 * @return void
 */
static void MFRC522_CalculateCRC(uint8_t *data, uint8_t len, uint8_t *result)
{
    /* Prepare CRC calculator */
    MFRC522_WriteRegister(CommandReg, PCD_IDLE);
    MFRC522_WriteRegister(DivIrqReg, 0x04);          /* Clear CRC IRQ flag */
    MFRC522_SetBitMask(FIFOLevelReg, 0x80);          /* Clear FIFO pointer */

    /* Write data to FIFO for CRC calculation */
    for (uint8_t i = 0; i < len; i++) {
        MFRC522_WriteRegister(FIFODataReg, data[i]);
    }
    
    /* Trigger CRC calculation */
    MFRC522_WriteRegister(CommandReg, PCD_CALCCRC);

    /* Wait for CRC calculation to complete (with timeout) */
    uint16_t timeout = 5000;
    uint8_t status;
    do {
        status = MFRC522_ReadRegister(DivIrqReg);
        timeout--;
    } while (timeout && !(status & 0x04));

    /* Read 2-byte CRC result */
    result[0] = MFRC522_ReadRegister(CRCResultRegL);
    result[1] = MFRC522_ReadRegister(CRCResultRegM);
}

/* ============================================================================
 * Register I/O Functions
 * ============================================================================ */

void MFRC522_WriteRegister(uint8_t reg, uint8_t value)
{
    /* SPI protocol: addr = (reg << 1 & 0x7E) with MSB=0 for write */
    uint8_t addr = ((reg << 1) & 0x7E);
    uint8_t tx[2] = { addr, value };
    
    MFRC522_Select();
    HAL_SPI_Transmit(&hspi1, tx, 2, HAL_MAX_DELAY);
    MFRC522_Unselect();
}

uint8_t MFRC522_ReadRegister(uint8_t reg)
{
    /* SPI protocol: addr = (reg << 1 & 0x7E) | 0x80 for read (set MSB=1) */
    uint8_t addr = ((reg << 1) & 0x7E) | 0x80;
    uint8_t tx = addr;
    uint8_t rx = 0;
    
    MFRC522_Select();
    HAL_SPI_Transmit(&hspi1, &tx, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, &rx, 1, HAL_MAX_DELAY);
    MFRC522_Unselect();
    
    return rx;
}

void MFRC522_SetBitMask(uint8_t reg, uint8_t mask)
{
    /* Read register, set bits (logical OR), write back */
    uint8_t tmp = MFRC522_ReadRegister(reg);
    MFRC522_WriteRegister(reg, tmp | mask);
}

void MFRC522_ClearBitMask(uint8_t reg, uint8_t mask)
{
    /* Read register, clear bits (logical AND with NOT mask), write back */
    uint8_t tmp = MFRC522_ReadRegister(reg);
    MFRC522_WriteRegister(reg, tmp & (~mask));
}

/* ============================================================================
 * Device Control Functions
 * ============================================================================ */

void MFRC522_Reset(void)
{
    /* Perform soft reset using PCD_RESETPHASE command */
    MFRC522_WriteRegister(CommandReg, PCD_RESETPHASE);
    HAL_Delay(MFRC522_RESET_DELAY_MS);
}

void MFRC522_AntennaOn(void)
{
    /* Enable TX pins (antenna) by setting bits 0-1 of TxControlReg */
    uint8_t value = MFRC522_ReadRegister(TxControlReg);
    if (!(value & 0x03)) {
        MFRC522_SetBitMask(TxControlReg, 0x03);
    }
}

void MFRC522_AntennaOff(void)
{
    /* Disable TX pins (antenna) by clearing bits 0-1 of TxControlReg */
    MFRC522_ClearBitMask(TxControlReg, 0x03);
}

void MFRC522_Init(void)
{
    /* Ensure CS is high (unselected) on startup */
    MFRC522_Unselect();

    /* Hardware reset via RST pin */
    HAL_GPIO_WritePin(MFRC522_RST_PORT, MFRC522_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(MFRC522_INIT_DELAY_MS);

    /* Soft reset via command register */
    MFRC522_Reset();
    HAL_Delay(MFRC522_INIT_DELAY_MS);

    /* Timer configuration for card presence timeout */
    MFRC522_WriteRegister(TModeReg, 0x8D);     /* TPrescaler*Reload timing mode */
    MFRC522_WriteRegister(TPrescalerReg, 0x3E);
    MFRC522_WriteRegister(TReloadRegL, 30);
    MFRC522_WriteRegister(TReloadRegH, 0);

    /* Modulation configuration */
    MFRC522_WriteRegister(TxASKReg, 0x40);     /* 100% ASK modulation */
    MFRC522_WriteRegister(ModeReg, 0x3D);      /* CRC initial value 0x6363 */

    /* Enable antenna for RF transmission */
    MFRC522_AntennaOn();
}

/* ============================================================================
 * Protocol Functions
 * ============================================================================ */

uint8_t MFRC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen,
                       uint8_t *backData, uint16_t *backLen)
{
    uint8_t status = MI_ERR;
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;
    uint8_t lastBits;
    uint8_t n;
    uint16_t i;

    /* Configure interrupt and wait flags based on command type */
    if (command == PCD_AUTHENT) {
        irqEn = 0x12;
        waitIRq = 0x10;
    } else if (command == PCD_TRANSCEIVE) {
        irqEn = 0x77;
        waitIRq = 0x30;
    }

    /* Enable interrupt request masking */
    MFRC522_WriteRegister(CommIEnReg, irqEn | 0x80);
    MFRC522_ClearBitMask(CommIrqReg, 0x80);    /* Clear all IRQ bits first */
    MFRC522_SetBitMask(FIFOLevelReg, 0x80);    /* Clear FIFO buffer */

    /* Load transmit data into FIFO */
    for (i = 0; i < sendLen; i++) {
        MFRC522_WriteRegister(FIFODataReg, sendData[i]);
    }

    /* Initiate command execution */
    MFRC522_WriteRegister(CommandReg, PCD_IDLE);
    MFRC522_SetBitMask(BitFramingReg, 0x80);   /* StartSend = 1 */
    MFRC522_WriteRegister(CommandReg, command);
    
    /* For transceive, ensure transmission starts */
    if (command == PCD_TRANSCEIVE) {
        MFRC522_SetBitMask(BitFramingReg, 0x80);
    }

    /* Wait for command completion or timeout (~2000 iterations) */
    i = 2000;
    do {
        n = MFRC522_ReadRegister(CommIrqReg);
        i--;
    } while (i && !(n & 0x01) && !(n & waitIRq));

    /* Clear transmission start bit */
    MFRC522_ClearBitMask(BitFramingReg, 0x80);

    /* Process result if command completed */
    if (i != 0) {
        uint8_t errorReg = MFRC522_ReadRegister(ErrorReg);
        
        /* Check for errors in ErrorReg (bits 0-4) */
        if (!(errorReg & 0x1B)) {
            status = MI_OK;
            
            /* Check if response came back (MI_NOTAGERR = timeout/no response) */
            if (n & irqEn & 0x01) {
                status = MI_NOTAGERR;
            }

            /* For transceive commands, extract received data */
            if (command == PCD_TRANSCEIVE) {
                /* Get FIFO level and bit position of last byte */
                n = MFRC522_ReadRegister(FIFOLevelReg);
                lastBits = MFRC522_ReadRegister(ControlReg) & 0x07;
                
                /* Calculate response length in bits */
                if (lastBits) {
                    *backLen = (n - 1) * 8 + lastBits;
                } else {
                    *backLen = n * 8;
                }

                /* Read response data from FIFO (max 16 bytes) */
                if (n == 0) n = 1;
                if (n > 16) n = 16;
                for (uint8_t idx = 0; idx < n; idx++) {
                    backData[idx] = MFRC522_ReadRegister(FIFODataReg);
                }
            }
        } else {
            status = MI_ERR;
        }
    }

    return status;
}

uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *TagType)
{
    uint8_t status;
    uint16_t backBits;

    /* Set last bits = 7 (send 7 bits of request byte) */
    MFRC522_WriteRegister(BitFramingReg, 0x07);

    /* Send request command (PICC_REQIDL or PICC_REQALL) */
    TagType[0] = reqMode;
    status = MFRC522_ToCard(PCD_TRANSCEIVE, TagType, 1, TagType, &backBits);

    /* Valid response is 16 bits (0x10 bits) */
    if ((status != MI_OK) || (backBits != 0x10)) {
        status = MI_ERR;
    }

    return status;
}

uint8_t MFRC522_Anticoll(uint8_t *serNum)
{
    uint8_t status;
    uint8_t i;
    uint8_t serNumCheck = 0;
    uint16_t unLen;

    /* Send anticollision command (Cascade Level 1) */
    MFRC522_WriteRegister(BitFramingReg, 0x00);

    uint8_t buf[2];
    buf[0] = PICC_ANTICOLL;
    buf[1] = 0x20;
    status = MFRC522_ToCard(PCD_TRANSCEIVE, buf, 2, serNum, &unLen);

    /* Response should contain 5 bytes: 4-byte UID + 1-byte checksum */
    if (status == MI_OK) {
        /* Verify BCC (Block Check Character): XOR of UID bytes should equal BCC */
        for (i = 0; i < 4; i++) {
            serNumCheck ^= serNum[i];
        }
        if (serNumCheck != serNum[4]) {
            status = MI_ERR;
        }
    }
    return status;
}

uint8_t MFRC522_SelectTag(uint8_t *serNum)
{
    uint8_t i;
    uint8_t buf[9];
    uint8_t size;
    uint8_t status;
    uint16_t recvBits;

    /* Build SELECT command packet: CMD + NVB + UID[4] + CRC[2] */
    buf[0] = PICC_SELECT_TAG;
    buf[1] = 0x70;
    for (i = 0; i < 5; i++) {
        buf[i + 2] = serNum[i];
    }

    /* Calculate and append CRC16 */
    uint8_t crc[2];
    MFRC522_CalculateCRC(buf, 7, crc);
    buf[7] = crc[0];
    buf[8] = crc[1];

    /* Send SELECT command and get SAK (Select Acknowledge) */
    status = MFRC522_ToCard(PCD_TRANSCEIVE, buf, 9, buf, &recvBits);
    
    /* Valid SAK response is 8 bits (0x18 = 24 bits = 3 bytes with 0 padding?) */
    if ((status == MI_OK) && (recvBits == 0x18)) {
        size = buf[0];
        return size;
    } else {
        return 0;
    }
}

void MFRC522_Halt(void)
{
    /* Send PICC_HALT command to put card into low-power idle state */
    uint8_t buff[4];
    buff[0] = PICC_HALT;
    buff[1] = 0;
    
    /* Calculate and append CRC16 */
    uint8_t crc[2];
    MFRC522_CalculateCRC(buff, 2, crc);
    buff[2] = crc[0];
    buff[3] = crc[1];

    /* Send halt command (ignore response) */
    uint16_t tmp;
    (void)MFRC522_ToCard(PCD_TRANSCEIVE, buff, 4, buff, &tmp);
}

/* ============================================================================
 * Convenience / High-Level API
 * ============================================================================ */

uint8_t MFRC522_Check(uint8_t *id)
{
    uint8_t status;
    uint8_t tagtype[2];
    
    /* Step 1: Send request to detect card */
    status = MFRC522_Request(PICC_REQIDL, tagtype);
    if (status != MI_OK) {
        return MI_NOTAGERR;
    }

    /* Step 2: Anti-collision to get UID */
    status = MFRC522_Anticoll(id);
    if (status != MI_OK) {
        return MI_ERR;
    }

    /* Step 3: Select tag (optional, used for authentication in full implementation) */
    MFRC522_SelectTag(id);
    
    /* Step 4: Halt card to reduce power consumption */
    MFRC522_Halt();

    return MI_OK;
}
