# UNO R4 WiFi / WiFiS3 I2C Master

Experimental master for boards that use Arduino's `WiFiS3` library, mainly Arduino UNO R4 WiFi.

It supports:

- up to three WiFi profiles
- local master mining
- automatic I2C slave scanning
- optional ESP-style co-mining behavior through `master_mines_with_i2c_slaves`

## Libraries

Install:

```text
WiFiS3
DuinoCoin
Wire
```

The UNO R4 WiFi is not expected to match ESP32 speed. This folder is for portability and for rigs where the UNO R4 WiFi is the internet-connected controller.

## Upload

Open:

```text
DuinoCoin_UNO_R4_WiFi_I2C_Master.ino
```

Select an UNO R4 WiFi board in Arduino IDE, edit the settings near the top, then upload.
