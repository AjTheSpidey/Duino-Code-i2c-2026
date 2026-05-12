# RP2040 I2C Slave

RP2040 / Raspberry Pi Pico slave for the ESP and Raspberry Pi I2C masters.

Protocol:

```text
lastBlockHash,expectedHash,difficulty\n
```

Return format:

```text
nonce,elapsedMicros,DUCOID-RP2040-address\n
```

Notes:

- Use an Arduino-compatible RP2040 core.
- The selected `DuinoCoin` library must support your core.
- Default I2C address is `0x12`; change `I2C_ADDRESS` if you run more than one RP2040 slave.
- RP2040 is 3.3V only. Use level shifting with 5V boards.
