# RFID Cashless Wallet

## Project Overview

A contactless payment system built on the STM32F303RE microcontroller for campus use. Students tap their RFID cards to make purchases at registered vendors, while vendors use separate RFID cards to select items and prices. The system handles the full transaction flow — item selection, balance verification, payment confirmation, and receipt logging — entirely on embedded hardware with no internet or backend required. Built using the MFRC522 RFID reader over SPI, a 16x2 LCD for real-time feedback, and flash-based EEPROM emulation for persistent balance storage. Supports multiple vendors, low-balance alerts, transaction history, and automatic session timeouts.

## Hardware Requirements

- STM32F303RETx microcontroller board
- MFRC522 RFID reader module
- 16x2 character LCD in 4-bit mode
- Buzzer (active high)
- Two buttons: EXIT (active low) and CONFIRM (reserved for future use)
- UART2 serial connection for debugging (115200 baud)

## Pin Connections

### LCD (4-bit mode)

| Pin | Purpose | STM32F303RE |
|-----|---------|-----------|
| RS | Register Select | PB1 |
| EN | Enable/Strobe | PB2 |
| D4 | Data Bit 4 | PB10 |
| D5 | Data Bit 5 | PB11 |
| D6 | Data Bit 6 | PB12 |
| D7 | Data Bit 7 | PB13 |

### MFRC522 RFID Reader (SPI1)

| Pin | Purpose | STM32F303RE |
|-----|---------|-----------|
| CS | Chip Select (active low) | PA4 |
| SCK | Serial Clock | PA5 (SPI1 AF5) |
| MISO | Master In, Slave Out | PA6 (SPI1 AF5) |
| MOSI | Master Out, Slave In | PA7 (SPI1 AF5) |
| RST | Hardware Reset (active high) | PB12 |

### Buzzer

| Pin | Purpose | STM32F303RE |
|-----|---------|-----------|
| BUZZER | Buzzer Control (active high) | PA8 |

### Buttons

| Pin | Purpose | STM32F303RE | Mode |
|-----|---------|-----------|------|
| EXIT | Menu Exit Button | PC13 | Input with internal pull-up, active low |
| CONFIRM | Reserved for future | PC14 | Input with internal pull-up, not used |

### UART2 Debug Console

| Pin | Purpose | STM32F303RE |
|-----|---------|-----------|
| TX | Debug Output | PA9 |
| RX | Debug Input | PA10 |

## Circuit Diagram

![RFID Wallet Circuit Diagram](Circuit%20diagram.png)

*Complete hardware interconnection diagram showing STM32F303RE with MFRC522 RFID reader, 16x2 LCD, buzzer, and button connections.*

## Configuration

All system parameters (timing, GPIO pins, currency thresholds, wallet limits) are centralized in `Core/Inc/config.h`. 
Edit this file to adjust:

- **Timing constants** (card debounce count, hold-to-select time, menu timeout, buzzer durations)
- **Wallet limits** (max cards, max transactions per card, preload count)
- **Currency values** (in paise/cents: topup amount, debit amount, low-balance threshold)
- **GPIO pin definitions** for LCD, RFID reader, buzzer, and buttons
- **SPI and UART configuration** parameters

No magic numbers are embedded in source code; all configurable values use named constants from `config.h`.

## System Architecture

The RFID wallet firmware is organized into modular components:

- **Core/Inc/config.h** — Centralized configuration constants (timing, GPIO pins, limits, thresholds)
- **Core/Inc/main.h** — CubeMX-generated HAL definitions (auto-managed, do not edit)
- **Core/Src/main.c** — System initialization and peripheral setup (clock, GPIO, SPI, UART)
- **Core/Inc/lcd.h** — LCD driver public interface (4-bit mode, clear, print functions)
- **Core/Src/lcd.c** — LCD driver implementation (nibble bit-banging, timing control)
- **Core/Inc/mfrc522.h** — RFID reader public interface (SPI protocol, PICC commands, registers)
- **Core/Src/mfrc522.c** — RFID reader implementation (hardware initialization, card detection, anti-collision)
- **Core/Inc/wallet.h** — Wallet data structures and transaction API (circular buffer history, balance)
- **Core/Src/wallet.c** — Wallet storage implementation (card slots, balance tracking, transaction log)
- **Core/Inc/state_machine.h** — Application state enumeration
- **Core/Src/state_machine.c** — Main application logic (card detection, menu navigation, hold-to-select, timeouts)
- **Core/Src/eeprom_emul.c** — Flash-based EEPROM emulator (persistent balance storage via CubeMX-generated code)
- **Drivers/** — STM32 HAL libraries (vendor-provided, not modified)

## Application Flow

1. **Initialization** → System clock (72 MHz), GPIO, SPI1, UART2, MFRC522, LCD, wallet data
2. **Idle State** → Displays "Ready - Tap Card", polls MFRC522 for card presence (80 ms loop)
3. **Card Detection** → Debounces card presence, prevents false positives (CARD_STABLE_COUNT=2 detections)
4. **Menu Entry** → Displays "Welcome [Name]", shows menu option on LCD (Balance, Add 10.00, Sub 1.00, History, Exit)
5. **Menu Navigation** → Tap card again to cycle through options (wraps around at end)
6. **Hold-to-Select** → Hold card for MENU_HOLD_TIME_MS (800 ms) to confirm selection
7. **Action Execution** → Updates balance, displays transaction result, plays buzzer feedback
8. **Low Balance Alert** → If balance < LOW_BALANCE_THRESHOLD_PAISE (100), urgent buzzer and display warning
9. **Exit Menu** → 30-second timeout after last activity or manual EXIT button press
10. **Return to Idle** → Card removed, display "Ready - Tap Card" again

## How to Build

1. Open `rfidf.ioc` in STM32CubeIDE.
2. Generate the project code if prompted.
3. Build the project using the IDE build button.

## How to Flash

1. Connect the STM32F303RE board to your PC with a suitable ST-LINK or compatible debugger.
2. In STM32CubeIDE, select the debug configuration for this project.
3. Click `Run` or `Debug` to program the device.

## Project Structure

- `Core/Inc/` - Project headers for LCD, RFID, wallet, EEPROM, and state machine.
- `Core/Src/` - Main application source files and drivers for the wallet state machine.
- `Drivers/` - HAL and CMSIS vendor libraries (not modified).
- `rfidf.ioc` - CubeMX configuration file.
- `STM32F303RETX_FLASH.ld` - Linker script.

## Known Limitations

- The student roster is RAM-only; card registrations are hardcoded.
- Item prices and wallet flows are hardcoded in firmware.
- No persistent wallet storage or user registration via runtime input.

## Author

Group Project — SNU ECE, November 2025
