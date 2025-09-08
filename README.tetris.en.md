# Tetris (tetris.py) Run Guide

Instructions to prepare the environment and run the Python script `tetris.py` that drives the 7‑segment LED panel.

## Overview
- Operate the game in your terminal (curses) while rendering the board on an I2C 7‑segment LED panel.
- Supports both a setup with the TCA9548A I2C multiplexer and a direct (no‑mux) setup. See `config.json`.

## Requirements
- OS: Linux
- Python 3.8+
- pip (Python package manager)
- I2C enabled and a device node present (e.g., `/dev/i2c-0` or `/dev/i2c-1`)
- Python package: `smbus2`
- (Optional) I2C tools: `i2c-tools` (`i2cdetect` to verify devices)

## Enable and verify I2C
1) Enable I2C (use your board's config tool / device tree as appropriate)
2) Check device nodes
   ```bash
   ls -l /dev/i2c*
   # e.g. if /dev/i2c-0 exists use bus=0, if /dev/i2c-1 exists use bus=1
   ```
3) (Optional) I2C scan
   ```bash
   sudo apt-get update
   sudo apt-get install -y i2c-tools
   i2cdetect -l           # list available buses
   i2cdetect -y 0         # scan bus 0 (change number to your bus)
   ```

> Note: `tetris.py` uses I2C bus 0 by default (`SMBus(0)`). If your system uses `/dev/i2c-1`, change `SMBus(0)` to `SMBus(1)` in `tetris.py`.

## Setup (recommended: virtualenv)
```bash
# From repo root
python3 -m venv venv
source venv/bin/activate
pip install --upgrade pip
pip install smbus2
```

Permissions
- If you cannot access `/dev/i2c-*`, either:
  - Run with sudo temporarily, or
  - Add your user to the `i2c` group and re‑login:
    ```bash
    sudo usermod -aG i2c $USER
    # log out and back in to apply
    ```

## How to run
- Basic:
  ```bash
  python tetris.py --config 16x12
  ```
- `--config` is one of the names under `configurations` in `config.json`.
  - Examples: `24x4`, `12x8`, `16x8`, `16x12`, `48x4`, `12x8-direct`
- The board size is defined by the chosen config (e.g., `16x12` → 16×12 digits).

## Controls
- Left / Right: move horizontally
- Down: soft‑drop one cell (resets auto‑drop timer)
- Up or Space: rotate
- q: quit

## Config file (config.json) notes
- Pick a configuration name from `configurations` and pass it via `--config`.
- Addresses are hex strings (e.g., `"0x70"`) and are converted to integers in the script.
- With TCA9548A, set `tca9548a_address` to a hex string (e.g., `"0x77"`).
- For direct setups, set `tca9548a_address: null` and use `-1` as the key in `channel_grids`.
- Example: `16x12` uses 4×4‑digit modules arranged 4 (wide) × 3 (high) → total 16×12 digits.

## Troubleshooting
- I2C bus not found (`I2C bus device not found`)
  - Check that `/dev/i2c-*` exists. If your bus is 1, change to `SMBus(1)`.
  - Ensure I2C is enabled in the system.
- I/O errors (`I2C Write Error` or init failures)
  - Verify wiring, power, addresses (with `i2cdetect`), and TCA9548A channel selection.
  - Ensure `channel_grids` in `config.json` matches the actual wiring.
- Terminal issues (black/garbled output)
  - Ensure the terminal has enough width; otherwise visible ASCII render will wrap.
- Display not cleared on exit
  - In rare exceptions cleanup may fail; running the script again typically clears the display.

## Notes
- Auto‑drop speed increases with score (see constants near the top of the file to tune).
- Board rendering uses `'*'` and `'#'` (mapping to 7‑segment via `digit_map`).

---
Minimal example (16x12, bus 0)
```bash
# First‑time deps
python3 -m venv venv
source venv/bin/activate
pip install smbus2

# Run
python tetris.py --config 16x12
```

Bus 1 environment (requires code edit)
```text
# In tetris.py
- bus = SMBus(0)
+ bus = SMBus(1)
```
