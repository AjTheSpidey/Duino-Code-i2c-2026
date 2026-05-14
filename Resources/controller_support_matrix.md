# Controller Support Matrix

This repo is organized by controller role rather than by one universal sketch.

## Main targets

| Folder | Hardware | Internet | Local mining | I2C slaves | Status |
|---|---|---:|---:|---:|---|
| `DuinoCoin_Esp_Async_Master` | ESP8266, ESP32, ESP32-S3, ESP32-S2/C3 class boards | WiFi | Yes | Yes | Main |
| `DuinoCoin_RaspberryPi_I2C_Master` | Raspberry Pi Zero/3/4/5 and similar Linux SBCs | WiFi/Ethernet | Yes, multi-process | Yes | Main |
| `DuinoCoin_Arduino_Slave` | Uno, Nano, Mega, Leonardo, Micro, Pro Mini | No | I2C slave miner | N/A | Main |

## Added portability targets

| Folder | Hardware | Internet | Local mining | I2C slaves | Status |
|---|---|---:|---:|---:|---|
| `DuinoCoin_PicoW_WiFi_I2C_Master` | Raspberry Pi Pico W, RP2040 WiFi boards running MicroPython | WiFi | Yes | Yes | Experimental |
| `DuinoCoin_Arduino_WiFiNINA_I2C_Master` | MKR WiFi 1010, Nano 33 IoT, Nano RP2040 Connect with WiFiNINA | WiFi | Yes | Yes | Experimental |
| `DuinoCoin_UNO_R4_WiFi_I2C_Master` | Arduino UNO R4 WiFi and WiFiS3 boards | WiFi | Yes | Yes | Experimental |
| `DuinoCoin_PC_I2C_Bridge_Experimental` | PC plus USB-connected bridge MCU | PC internet | No local PC miner in this script | Yes | Experimental |

## High-power non-WiFi slaves

| Folder | Hardware | Role | Status |
|---|---|---|---|
| `DuinoCoin_STM32_I2C_Slave` | STM32F/H/G boards with Arduino-compatible cores | I2C mining slave | Experimental |
| `DuinoCoin_RP2040_I2C_Slave` | Pico / RP2040 boards | I2C mining slave | Experimental |
| `DuinoCoin_Tiny_Slave` | ATtiny and very small AVR boards | I2C mining slave | Experimental |

## WiFi microcontroller coverage

Covered directly:

- ESP8266
- ESP32, ESP32-S2, ESP32-S3, ESP32-C3 style Arduino-core boards
- Raspberry Pi Pico W / RP2040 WiFi through MicroPython
- Arduino WiFiNINA boards
- Arduino UNO R4 WiFi / WiFiS3 boards

Useful through the PC bridge:

- STM32 without WiFi
- RP2040/Pico without WiFi
- Teensy without WiFi
- AVR boards without WiFi
- any board that can act as an I2C slave using the `job -> result` line protocol

Not included as full masters yet:

- ESP32 with Ethernet modules
- Teensy 4.1 Ethernet
- STM32 boards with external ESP-AT or Wiznet networking
- nRF52 boards with add-on WiFi

Those can still be used as slaves or as bridge controllers. They need board-specific networking libraries before a clean WiFi master sketch makes sense.

## Recommended choices

Use ESP32/S3 when you want the most complete microcontroller master. Use ESP8266 when cheap WiFi and basic I2C control matter more than maximum speed. Use Raspberry Pi when you want the most flexible multi-core local mining plus I2C control. Use the PC bridge when the microcontrollers do not have WiFi.
