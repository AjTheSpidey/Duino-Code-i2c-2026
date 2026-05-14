# Arduino WiFiNINA I2C Master

Experimental Arduino master for WiFiNINA boards:

- Arduino MKR WiFi 1010
- Arduino Nano 33 IoT
- Arduino Nano RP2040 Connect when using the Arduino WiFiNINA stack

It follows the same idea as the ESP master: the WiFi board can mine locally and can also feed I2C slaves.

## Status

This is a portability sketch. ESP32 is still the preferred WiFi master for speed, async web support, and dual-core mining.

## Libraries

Install:

```text
WiFiNINA
DuinoCoin
Wire
```

## Settings

Edit the obvious settings near the top of `DuinoCoin_Arduino_WiFiNINA_I2C_Master.ino`:

```cpp
const char* user = "your_username";
const char* rig = "WiFiNINA-I2C-2026";
const char* wifi_1_name = "your_wifi_name";
const char* wifi_1_pass = "your_wifi_password";
```

`master_only`, `auto_i2c_slaves`, and `master_mines_with_i2c_slaves` work like the ESP sketch.

## Wiring

Use the board's normal SDA/SCL pins. Most WiFiNINA boards are 3.3V. Use level shifting with 5V AVR slaves.
