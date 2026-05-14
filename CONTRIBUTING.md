# Contributing

Contributions are welcome, especially real test results from mixed rigs.

## How to contribute

1. Fork this repository.
2. Create a branch in your fork with a short name for your change.
3. Make your code or documentation update.
4. Test it on the actual board or setup if you can.
5. Open a pull request back to this repository.

In the pull request, include:

- what board or controller you used
- what slaves were connected, if any
- what feature was added or fixed
- what settings you changed
- compile or runtime results
- before/after hashrate, ping, or stability notes if the change affects performance

## Good changes

Useful pull requests include:

- new controller folders for boards with WiFi, Ethernet, USB bridge, or strong I2C support
- fixes for compile errors on specific board cores
- better defaults for real hardware
- board detection improvements
- clearer READMEs or wiring notes
- tested I2C performance improvements

## Keep public code safe

Do not commit real WiFi passwords, mining keys, or private account details. Use placeholders in public examples:

```cpp
const char* user = "your_username";
const char* wifi_1_name = "your_wifi_name";
const char* wifi_1_pass = "your_wifi_password";
```

## Testing notes

When testing performance, include enough detail for someone else to repeat it:

```text
Board: ESP8266 NodeMCU
Slaves: 2x Arduino Nano old bootloader
I2C clock: 100000
master_mines_with_i2c_slaves: true
master_hash_us_shared: 3000
Average slave rate: 205 H/s each
Master rate: 5-12 kH/s
Observed issue/fix: fewer timeouts after lowering I2C clock
```

Small practical results are more useful than perfect benchmark tables.
