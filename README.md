# Duino-Code I2C 2026

Duino-Code I2C 2026 is a Duino-Coin I2C mining setup for mixed small-board rigs.

The main build is an ESP8266/ESP32 master that mines on its own and also watches the I2C bus for AVR miners. If no slaves are present, the ESP keeps mining as a normal standalone miner. If slaves are plugged in later, it detects them and starts feeding them jobs while the ESP continues mining locally.

## What is included

- ESP8266/ESP32 automatic I2C master miner
- Arduino AVR I2C slave miner for Uno, Nano, Mega, Leonardo, Micro, and similar boards
- Raspberry Pi automatic I2C master
- STM32 I2C slave sketch
- RP2040 / Pico I2C slave sketch
- Experimental notes for master-to-master bridge setups

## ESP master behavior

The ESP master always mines. I2C is automatic.

```text
No slaves detected  -> ESP mines mostly like a normal standalone ESP miner
Slaves detected     -> ESP mines and also controls the AVR I2C miners
Slaves removed      -> ESP returns to mostly solo mining
```

The I2C scan is cached so the sketch does not waste time probing every address on every loop.

## Easy settings

Open:

```text
DuinoCoin_Esp_Async_Master/DuinoCoin_Esp_Async_Master.ino
```

Edit the block near the top:

```cpp
const char* user = "your_username";
const char* rig = "Duino-I2C-2026";
const char* mining_key = "";

const char* wifi_1_name = "your_wifi_name";
const char* wifi_1_pass = "your_wifi_password";
const char* wifi_2_name = "";
const char* wifi_2_pass = "";
const char* wifi_3_name = "";
const char* wifi_3_pass = "";

const bool auto_i2c_slaves = true;
const bool master_turbo_when_solo = true;
const bool master_use_second_core = true;
const byte max_avr_miners = 16;
const unsigned long master_hash_us_single = 250000;
const unsigned long master_hash_us_shared = 20000;
const unsigned long i2c_scan_empty_ms = 15000;
const unsigned long i2c_scan_active_ms = 5000;
const unsigned long i2c_read_timeout_ms = 8;
const unsigned long i2c_wire_clock = 400000;
```

Three WiFi profiles are supported. If all WiFi names are empty, the ESP tries saved WiFi credentials.

For fastest solo mining, keep `master_turbo_when_solo` enabled and increase `master_hash_us_single` carefully. Bigger
values mine harder when no I2C slaves are online, but web dashboard and OTA updates will respond less often.

On dual-core ESP32 boards, including ESP32-S3, `master_use_second_core` starts a second hash lane on the other core.
The main loop still services WiFi, web, OTA, and I2C, while the extra core searches the alternate nonce lane.

## ESP master upload files

Open this folder in Arduino IDE:

```text
DuinoCoin_Esp_Async_Master
```

Keep all files in that folder:

```text
DuinoCoin_Esp_Async_Master.ino
DuinoCoin_MasterMiner.ino
DuinoCoin_Clients.ino
DuinoCoin_Wire.ino
DuinoCoin_Pool.ino
DuinoCoin_oled.ino
DoinoCoin_AsyncWebServer.ino
DSHA1.h
Counter.h
```

Libraries commonly needed:

```text
ESPAsyncWebServer
ESPAsyncTCP or AsyncTCP
LittleFS
ESP8266 and ESP32 OLED driver for SSD1306 displays by ThingPulse
StreamString
```

The ESP master was compile-checked with `esp32:esp32:esp32`, `esp32:esp32:esp32s3`, and
`esp8266:esp8266:nodemcuv2`.

## AVR slave upload files

Open this folder in Arduino IDE:

```text
DuinoCoin_Arduino_Slave
```

Keep these files together:

```text
DuinoCoin_Arduino_Slave.ino
DuinoCoin_Utils.ino
DuinoCoin_Wire.ino
```

Libraries commonly needed:

```text
DuinoCoin
ArduinoUniqueID
StreamString
Wire
```

The slave reports a board tag in its ID when the Arduino core exposes it, for example `UNO`, `NANO`, or `MEGA2560`.

## I2C wiring notes

- ESP8266 default I2C: SDA `GPIO4/D2`, SCL `GPIO5/D1`
- ESP32 default I2C: SDA `GPIO21`, SCL `GPIO22`
- AVR Uno/Nano: SDA `A4`, SCL `A5`
- All boards need common ground
- Use level shifting when mixing 3.3V masters with 5V slaves
- Keep wires short when running many boards

## Other folders

```text
DuinoCoin_RaspberryPi_I2C_Master
```

Python Raspberry Pi master. It has the same basic idea: local mining plus I2C slave control.

The Pi version uses separate workers and separate pool connections so it keeps mining locally while it serves I2C slaves.

```text
DuinoCoin_STM32_I2C_Slave
DuinoCoin_RP2040_I2C_Slave
```

Slave sketches for non-AVR boards using the same line protocol as the AVR slave. Library support varies by board core.

```text
DuinoCoin_MasterBridge_Experimental
```

Notes and a small serial status stub for master-to-master rigs.

## Status

This is a practical 2026 refresh of the older ESP I2C master idea. The ESP and Raspberry Pi master paths are the main targets. STM32, RP2040, and bridge folders are included for more specialized builds.
