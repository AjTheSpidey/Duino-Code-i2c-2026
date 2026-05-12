# DuinoCoin Raspberry Pi I2C Master

This is a Raspberry Pi version of the ESP I2C master idea.

Modes:
- `single`: Pi mines by itself.
- `i2c`: Pi only controls I2C AVR slaves.
- `both`: Pi mines and controls I2C AVR slaves.

Install:

```bash
python3 -m pip install smbus2
```

Edit `config.example.json`, save it as `config.json`, then run:

```bash
python3 raspi_i2c_master.py
```

Wiring notes:
- Raspberry Pi SDA: GPIO2 / physical pin 3
- Raspberry Pi SCL: GPIO3 / physical pin 5
- Common GND is required.
- AVR slaves should use level shifting if they run at 5V.

