# Raspberry Pi I2C Master

This is the Raspberry Pi version of the automatic ESP master.

It always mines locally. If `auto_i2c_slaves` is enabled, a controller worker scans the I2C bus and serves AVR-compatible slaves as they appear.

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
Master only true    -> Pi uses local mining workers only and skips I2C
```

Performance settings in `config.json`:

```json
"master_only": false,
"use_all_cores": true,
"local_mining_processes": 0,
"hash_yield_master_only": 0
```

`local_mining_processes: 0` means auto-detect CPU count. The local miners use processes instead of threads so a
multi-core Raspberry Pi can actually use more than one CPU core for hashing.

Wiring:

- Raspberry Pi SDA: GPIO2 / physical pin 3
- Raspberry Pi SCL: GPIO3 / physical pin 5
- Common GND is required
- Use a level shifter with 5V AVR boards

The Pi version uses separate Duino-Coin pool connections so local mining does not stop while I2C slaves are being served.
