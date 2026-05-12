# DuinoCoin RP2040 I2C Slave

Starter for Raspberry Pi Pico / RP2040 boards using Arduino-Pico or Mbed RP2040 cores.

This follows the same line protocol as the AVR slave:

```text
lastBlockHash,expectedHash,difficulty\n
```

It returns:

```text
nonce,elapsedMicros,DUCOID-RP2040-address\n
```

The actual `Ducos1a.work(...)` call depends on whether your selected DuinoCoin Arduino library supports RP2040. If it does not, use the ESP master's `DSHA1.h` and `Counter.h` pattern as the hashing core.

