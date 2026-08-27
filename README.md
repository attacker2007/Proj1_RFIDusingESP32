# Proj1_RFIDusingESP32

## Introduction

This is a mini-project that utilizes the RFID-RC522 module to build an access card attendance system. The project is built on a DOIT ESP32 DEVKIT V1, but it is also possible to create a simplified version on an Arduino Uno because the basic RFID functionality does not require much computing power or wireless functionality. The current sketch, however, is written for the ESP32 because it uses Bluetooth functionality.

The list of equipment is as follows:

- DOIT ESP32 DEVKIT V1 + Expansion Board (optional)

- RFID-RC522 reader module

- RFID card or key fob

- Breadboard (Mini)

- Male-Female Jumpers

- Buzzer

- Micro-USB to USB-A Wire Connector

## About

The system reads RFID card inputs and records whether each card is checked in or checked out. The output is sent to the Serial Monitor and, when connected, to a Bluetooth terminal. The card records are stored in the ESP32's memory while the device is running.

This project is intended as a small learning project for RFID, SPI communication, Bluetooth communication, and basic attendance tracking. It does not currently send information to an online attendance form or database.

## Features

- Reads RFID card UIDs using an RC522 reader.

- Registers up to five cards while the device is running.

- Uses the first scan of a card as a **time in** event.

- Uses a later scan of the same card as a **time out** event.

- Prevents a card from being checked out immediately after being checked in.

- Provides different buzzer sounds for successful scans and rejected scans.

- Prints attendance information to the Serial Monitor.

- Sends the same information to a connected Bluetooth terminal.

- Supports simple Bluetooth commands for checking status, clearing records, and displaying help.

## How It Works

When a card is scanned for the first time, its UID is registered in an available slot and the card is marked as checked in. The system records the time using the ESP32 uptime value provided by `millis()`.

When the same card is scanned again, the system checks how long it has been checked in. If the minimum checkout time has passed, the card is marked as checked out and the total duration is displayed. If the card is scanned too soon, the checkout is rejected.

The current minimum checkout time is 10 seconds. This value is mainly useful for preventing accidental double scans while testing the project.

## Hardware Connections

The following pin configuration is used in `RFID_Attendance.ino`.

### RC522 to ESP32

| RC522 Pin | ESP32 Pin | Purpose |
| --- | --- | --- |
| 3.3V | 3V3 | Power supply |
| GND | GND | Ground |
| SDA / SS | GPIO 21 | SPI chip select |
| RST | GPIO 4 | Reset |
| SCK | GPIO 18 | SPI clock |
| MOSI | GPIO 23 | SPI data from ESP32 to RC522 |
| MISO | GPIO 19 | SPI data from RC522 to ESP32 |

The `SDA` pin on many RC522 breakout boards is used as the SPI chip-select pin and is referred to as `SS` in the sketch. The RC522 should be powered from **3.3 V**. The RC522 communicates with the microcontroller over SPI.[1]

### Buzzer to ESP32

| Buzzer Pin | ESP32 Pin | Purpose |
| --- | --- | --- |
| Positive / signal | GPIO 25 | Buzzer output |
| Negative | GND | Ground |

The exact connection may depend on the type of buzzer being used. If the buzzer requires more current than an ESP32 GPIO pin can provide, use an appropriate transistor driver circuit instead of connecting it directly.

## Software Requirements

The project can be uploaded using the [Arduino IDE](https://www.arduino.cc/en/software). The ESP32 board support package is also required because the sketch uses ESP32-specific Bluetooth functionality.[2]

The following software and library are required:

| Software or library | Purpose |
| --- | --- |
| Arduino IDE | Editing and uploading the sketch |
| ESP32 board package | Compiling code for the ESP32 |
| `MFRC522` library | Communicating with the RC522 RFID reader |
| `SPI` library | SPI communication with the RC522; included with the Arduino platform |
| `BluetoothSerial` library | Bluetooth communication; included with the ESP32 Arduino core |

Install the `MFRC522` library through the Arduino IDE Library Manager, or download it from the [MFRC522 library repository](https://github.com/miguelbalboa/rfid).[3]

## Setup and Upload

1. Install the Arduino IDE and the ESP32 board package.

1. Install the `MFRC522` library using the Library Manager.

1. Connect the RC522, buzzer, and ESP32 according to the wiring tables above.

1. Open `RFID_Attendance.ino` in the Arduino IDE.

1. Select a suitable ESP32 board, such as **DOIT ESP32 DEVKIT V1** or **ESP32 Dev Module**.

1. Select the correct USB port for the ESP32.

1. Compile the sketch and upload it to the board.

1. Open the Serial Monitor and set the baud rate to **115200**.

1. Scan an RFID card near the RC522 reader.

After the ESP32 starts, the Serial Monitor should display a startup message similar to this:

```
========================================
   RFID Attendance System Ready
   Bluetooth: RFID-Attendance
   Min checkout time: 10s
========================================
```

## Bluetooth Usage

The ESP32 starts a Bluetooth device named `RFID-Attendance`. Pair a phone or computer with this device and open a Bluetooth terminal application. The terminal should send each command followed by a newline or carriage return.

The sketch sends attendance messages to both the USB Serial connection and the Bluetooth terminal when Bluetooth is connected.

### Bluetooth Commands

| Command | Function |
| --- | --- |
| `HELP` | Displays the available commands |
| `STATUS` | Displays the registered cards and their current state |
| `CLEAR` | Clears all registered card records |

Commands are not case-sensitive. For example, `status`, `STATUS`, and `Status` are treated as the same command.

## Configuration

The main settings can be changed near the beginning of `RFID_Attendance.ino`.

| Setting | Default value | Description |
| --- | --- | --- |
| `SS_PIN` | `21` | RC522 SPI chip-select pin |
| `RST_PIN` | `4` | RC522 reset pin |
| `SCK_PIN` | `18` | SPI clock pin |
| `MISO_PIN` | `19` | SPI MISO pin |
| `MOSI_PIN` | `23` | SPI MOSI pin |
| `BUZZER_PIN` | `25` | Buzzer output pin |
| `SCAN_COOLDOWN` | `2000` ms | Time between accepted card scans |
| `MIN_CHECKOUT` | `10000` ms | Minimum time before a card can check out |
| `MAX_CARDS` | `5` | Maximum number of cards stored in memory |
| `BT_DEVICE_NAME` | `RFID-Attendance` | Bluetooth device name shown during pairing |

For example, to change the minimum checkout time to one minute, update the following line:

```cpp
const unsigned long MIN_CHECKOUT = 60000;
```

The sketch currently uses uptime values instead of calendar time. Therefore, a displayed timestamp represents the time since the ESP32 started and is not a date or a real-world clock time.

## Buzzer Feedback

The buzzer is used to give simple feedback after a card scan.

| Event | Buzzer pattern |
| --- | --- |
| Time in | Two short beeps |
| Time out | One long beep |
| Rejected scan | Three short beeps |
| ESP32 startup | One long beep |

## Current Limitations

The card records are stored in RAM. They will be removed when the ESP32 is restarted, loses power, or receives the `CLEAR` Bluetooth command. The system can store a maximum of five active card records unless `MAX_CARDS` is changed in the sketch.

The system only identifies cards by their UID. It does not currently associate a UID with a person's name, save records to permanent storage, or use a real-time clock. It also does not connect to Wi-Fi or upload information to Google Sheets, an attendance form, or another online service.

The RC522 card UID should not be treated as a secure authentication method. UIDs can be copied or changed on some cards, so this project should not be used as the only security mechanism for doors, payment systems, or other sensitive access-control applications.[1]

## Troubleshooting

| Problem | Possible cause | Suggested solution |
| --- | --- | --- |
| `MFRC522.h: No such file or directory` | The RFID library is not installed | Install the `MFRC522` library through the Arduino IDE Library Manager |
| The RC522 does not detect cards | Incorrect wiring or power connection | Check the SPI pins, reset pin, chip-select pin, ground, and 3.3 V supply |
| The ESP32 does not upload the sketch | Incorrect board, port, or USB cable | Select the correct ESP32 board and port, and use a data-capable USB cable |
| The Bluetooth device is not visible | The board is not running the sketch or is not an ESP32 | Confirm that the upload completed and that the selected board is an ESP32 |
| A second scan is rejected | The minimum checkout time has not passed | Wait until the configured `MIN_CHECKOUT` period has elapsed |
| No attendance output is shown | The Serial Monitor baud rate is incorrect | Set the Serial Monitor to 115200 baud |
| The registry is full | Five card records are already active | Send `CLEAR` over Bluetooth or increase `MAX_CARDS` in the sketch |

If the RC522 continues to report communication errors, check the quality of the jumper connections and keep the SPI wires as short as possible. The MFRC522 library documentation also recommends checking the reader's power supply and using the correct voltage.[1]

## Project Structure

```
Proj1_RFIDusingESP32/
├── RFID_Attendance.ino
├── README.md
└── LICENSE
```

`RFID_Attendance.ino` contains the complete ESP32 sketch. `README.md` contains the project documentation, and `LICENSE` contains the MIT License text.

## Possible Improvements

Some possible improvements for future versions include adding permanent storage for card records, assigning names to card UIDs, using a real-time clock, and exporting attendance records to a file or online service. A display, LED indicators, or a web interface could also be added to make the system easier to use without opening a Serial Monitor or Bluetooth terminal.

## License

This project is licensed under the [MIT License](LICENSE). The license permits use, modification, and distribution of the project, subject to the conditions included in the license file.[4]

## References

[1]: [MFRC522 Arduino library documentation](https://github.com/miguelbalboa/rfid)[2]: [Arduino core for the ESP32](https://github.com/espressif/arduino-esp32)[3]: [MFRC522 library repository](https://github.com/miguelbalboa/rfid)[4]: [MIT License](https://opensource.org/license/mit/)

For the source code and the latest version of this project, visit the [Proj1_RFIDusingESP32 repository](https://github.com/attacker2007/Proj1_RFIDusingESP32).
