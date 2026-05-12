# DuinoCoin Master Bridge Experimental

This folder is for "masters controlling masters" setups.

Topology idea:

```text
Root ESP/Raspberry Pi master
  -> I2C sub-master address 40
      -> local AVR slaves 1..16
  -> I2C sub-master address 41
      -> local AVR slaves 1..16
```

This is intentionally experimental because a normal ESP I2C master cannot also be an I2C slave on the same bus without careful board/core support and bus isolation.

Recommended stable approach:
- Use Raspberry Pi as the root master.
- Give each ESP sub-master a serial or WiFi API instead of making it an I2C slave.
- Keep each sub-master responsible for its own local AVR I2C chain.

The protocol should stay line-based:

```text
PING\n
STATUS\n
PAUSE\n
RESUME\n
```

Return examples:

```text
OK,ESP32-SUBMASTER,3-SLAVES,120.4KH\n
```

