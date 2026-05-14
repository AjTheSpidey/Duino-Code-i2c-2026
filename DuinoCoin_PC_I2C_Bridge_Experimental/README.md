# PC I2C Bridge Experimental

This folder is for rigs where the microcontrollers do not have WiFi, but a PC connected by USB has internet.

The layout is:

```text
Duino-Coin pool
  -> PC running pc_i2c_bridge.py
      -> USB serial
          -> bridge MCU running Bridge_MCU_I2C_Master.ino
              -> I2C slaves: AVR, STM32, RP2040, Tiny, etc.
```

This is the practical version of "slave-to-slave" for I2C. I2C still needs a bus master, so the bridge MCU is the local I2C master. The PC is the internet master and pool client.

## Files

```text
Bridge_MCU_I2C_Master/Bridge_MCU_I2C_Master.ino
pc_i2c_bridge.py
config.example.json
```

## Bridge MCU

Open `Bridge_MCU_I2C_Master/Bridge_MCU_I2C_Master.ino` in Arduino IDE and upload it to a board connected to the PC over USB.

Good bridge choices:

- Arduino Mega
- Arduino Uno/Nano, for small rigs
- RP2040/Pico
- STM32
- Teensy

The bridge MCU does not need WiFi. It only needs USB serial and I2C.

## PC side

Install:

```text
pip install pyserial
```

Copy `config.example.json` to `config.json`, edit the serial port and account settings, then run:

```text
python pc_i2c_bridge.py
```

## Protocol

PC to bridge:

```text
SCAN
JOB,<address>,<lastBlockHash>,<expectedHash>,<difficulty>
READ,<address>,<timeoutMs>
```

Bridge to PC:

```text
ADDRS,1,2,3
OK
DATA,<slave response>
TIMEOUT
ERR,<message>
```

The slave response is the same as the ESP master expects:

```text
nonce,elapsedMicros,DUCOID-BOARD
```

## Notes

This keeps internet handling off the microcontrollers. It is useful for a pile of strong no-WiFi boards, or when WiFi is unstable and a PC can stay online more reliably.
