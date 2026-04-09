#ifndef __MFRC522_H
#define __MFRC522_H

#include "stm32f3xx_hal.h"

/* -------------------- USER PIN CONFIG -------------------- */

// Chip Select (SDA pin of RFID)
#define MFRC522_CS_PORT       GPIOA
#define MFRC522_CS_PIN        GPIO_PIN_4

// Reset Pin (your choice: PB12)
#define MFRC522_RST_PORT      GPIOB
#define MFRC522_RST_PIN       GPIO_PIN_12

/* --------------------------------------------------------- */

// MFRC522 Commands
#define PCD_IDLE              0x00
#define PCD_AUTHENT           0x0E
#define PCD_RECEIVE           0x08
#define PCD_TRANSMIT          0x04
#define PCD_TRANSCEIVE        0x0C
#define PCD_RESETPHASE        0x0F
#define PCD_CALCCRC           0x03

// PICC Commands
#define PICC_REQIDL           0x26
#define PICC_REQALL           0x52
#define PICC_ANTICOLL         0x93
#define PICC_SElECTTAG        0x93
#define PICC_AUTHENT1A        0x60
#define PICC_AUTHENT1B        0x61
#define PICC_READ             0x30
#define PICC_WRITE            0xA0
#define PICC_DECREMENT        0xC0
#define PICC_INCREMENT        0xC1
#define PICC_RESTORE          0xC2
#define PICC_TRANSFER         0xB0
#define PICC_HALT             0x50

// Status codes
#define MI_OK                 0
#define MI_NOTAGERR           1
#define MI_ERR                2

// MFRC522 Registers
#define CommandReg            0x01
#define CommIEnReg            0x02
#define DivIEnReg             0x03
#define CommIrqReg            0x04
#define DivIrqReg             0x05
#define ErrorReg              0x06
#define Status1Reg            0x07
#define Status2Reg            0x08
#define FIFODataReg           0x09
#define FIFOLevelReg          0x0A
#define ControlReg            0x0C
#define BitFramingReg         0x0D
#define ModeReg               0x11
#define TxModeReg             0x12
#define RxModeReg             0x13
#define TxControlReg          0x14
#define TxASKReg              0x15
#define CRCResultRegL         0x22
#define CRCResultRegM         0x21
#define TModeReg              0x2A
#define TPrescalerReg         0x2B
#define TReloadRegH           0x2C
#define TReloadRegL           0x2D

/* ------------------ Function Prototypes ------------------ */

void MFRC522_Init(void);

void MFRC522_WriteRegister(uint8_t reg, uint8_t value);
uint8_t MFRC522_ReadRegister(uint8_t reg);

void MFRC522_SetBitMask(uint8_t reg, uint8_t mask);
void MFRC522_ClearBitMask(uint8_t reg, uint8_t mask);

void MFRC522_AntennaOn(void);
void MFRC522_AntennaOff(void);

void MFRC522_Reset(void);

uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *TagType);
uint8_t MFRC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen,
                       uint8_t *backData, uint16_t *backLen);

uint8_t MFRC522_Anticoll(uint8_t *serNum);
uint8_t MFRC522_SelectTag(uint8_t *serNum);
void MFRC522_Halt(void);

#endif
