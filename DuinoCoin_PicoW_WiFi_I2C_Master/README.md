# Pico W WiFi I2C Master

Experimental MicroPython master for Raspberry Pi Pico W and other RP2040 boards with WiFi.

It can:

- connect to up to three WiFi networks
- mine locally when no I2C slaves are detected
- keep mining lightly while I2C slaves are active
- scan an I2C bus for AVR-compatible slaves
- forward jobs to I2C slaves and submit their shares to Duino-Coin

This is not as fast as the ESP32 C++ master. It is here for portability and for people who already have Pico W boards.

## Files

```text
picow_wifi_i2c_master.py
config.example.json
```

Copy `config.example.json` to `config.json`, edit the obvious settings, then upload both files to the Pico W filesystem.

## Pins

Default I2C pins:

```text
SDA: GP4
SCL: GP5
```

RP2040/Pico W is 3.3V only. Use a level shifter when the I2C slaves run at 5V.

## Notes

The Pico W has two cores, but MicroPython does not expose them in the same clean way that the ESP32 Arduino core does. This script uses cooperative scheduling instead of a hard second-core miner.
