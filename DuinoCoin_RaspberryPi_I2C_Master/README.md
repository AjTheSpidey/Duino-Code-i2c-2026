# Raspberry Pi I2C Master

This is the Raspberry Pi version of the automatic ESP master.

It always mines locally. If `auto_i2c_slaves` is enabled, a second worker scans the I2C bus and serves AVR-compatible slaves as they appear.

Install:

```bash
python3 -m pip install smbus2
```

Edit `config.example.json`, save it as `config.json`, then run:

```bash
python3 raspi_i2c_master.py
```

Behavior:

```text
No slaves detected  -> Pi keeps mining locally
Slaves detected     -> Pi keeps mining and also feeds I2C slaves
Slaves removed      -> Pi keeps mining and rescans in the background
```

Wiring:

- Raspberry Pi SDA: GPIO2 / physical pin 3
- Raspberry Pi SCL: GPIO3 / physical pin 5
- Common GND is required
- Use a level shifter with 5V AVR boards

The Pi version uses two workers and separate Duino-Coin pool connections so local mining does not stop while I2C slaves are being served.
