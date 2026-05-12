# STM32 I2C Slave

STM32 slave for the ESP and Raspberry Pi I2C masters.

It follows the same line protocol as the AVR slave:

```text
lastBlockHash,expectedHash,difficulty\n
```

It returns:

```text
nonce,elapsedMicros,DUCOID-STM32-address\n
```

Notes:

- Designed for Arduino-compatible STM32 cores.
- The selected `DuinoCoin` library must support your STM32 core.
- Leave `I2C_ADDRESS` as `0` for simple auto-addressing, or set a fixed address.
- Use 3.3V-safe wiring. Add a level shifter if the I2C master or other slaves run at 5V.
