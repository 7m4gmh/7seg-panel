#!/usr/bin/env python3
"""
led_sequencer.py

Reads config.json and runs a 7-segment lighting sequence:
 1) For every digit, light segments A..G in order with 0.5s delay
 2) Then, for each digit, turn all segments on for 0.5s while others off
 3) Repeat forever

Usage: python led_sequencer.py [--layout LAYOUT] [--delay 0.5]
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import time
from typing import Iterable, Tuple

try:
    from smbus2 import SMBus
except Exception:
    SMBus = None


SEGMENTS = ["A", "B", "C", "D", "E", "F", "G", "DP"]
DEBUG = False


def load_config(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def choose_layout(cfg: dict, preferred: str | None = None) -> dict:
    layouts = cfg.get("configurations", {})
    if not layouts:
        raise SystemExit("config.json contains no configurations")
    if preferred and preferred in layouts:
        return layouts[preferred]
    # prefer emulator layout if present
    for name in ("emulator-24x4", "emulator-12x8", "emulator-24x8"):
        if name in layouts:
            return layouts[name]
    # otherwise return the first layout
    first_key = next(iter(layouts))
    return layouts[first_key]


def build_digits_list(layout: dict) -> list[tuple[int, int]]:
    w = int(layout.get("total_width", 0))
    h = int(layout.get("total_height", 0))
    if w <= 0 or h <= 0:
        raise SystemExit("Invalid layout dimensions in config")
    digits: list[tuple[int, int]] = []
    for row in range(h):
        for col in range(w):
            digits.append((row, col))
    return digits


def main() -> None:
    p = argparse.ArgumentParser(description="7-seg LED sequencer from config.json")
    p.add_argument("--layout", help="configuration name in config.json to use")
    p.add_argument("--delay", help="delay seconds between steps", type=float, default=0.5)
    p.add_argument("--mode", choices=["print", "i2c"], default="print", help="operation mode: print (default) or i2c (write to HT16K33 devices)")
    p.add_argument("--debug", action="store_true", help="print per-module display buffers before I2C write")
    args = p.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_path = os.path.join(script_dir, "config.json")
    if not os.path.exists(config_path):
        print(f"config.json not found at {config_path}")
        sys.exit(1)

    cfg = load_config(config_path)
    layout = choose_layout(cfg, args.layout)
    digits = build_digits_list(layout)

    delay = float(args.delay)

    print(f"Using layout: {layout.get('name', 'unknown')} ({len(digits)} digits) delay={delay}s")

    mode = args.mode
    global DEBUG
    DEBUG = bool(args.debug)

    try:
        modules = []
        if mode == "i2c":
            if SMBus is None:
                print("smbus2 is not installed or unavailable. Install smbus2 to use --mode i2c.")
                sys.exit(1)
            modules = collect_ht16_modules(layout)
            if not modules:
                print("No HT16K33 modules detected in config; cannot run in i2c mode.")
                sys.exit(1)
            bus = SMBus(1)
            init_modules(bus, modules)

        while True:
            # Phase 1: for every digit, light segments A..G sequentially
            for idx, (r, c) in enumerate(digits):
                for seg in SEGMENTS:
                    ts = time.strftime("%Y-%m-%d %H:%M:%S")
                    if mode == "print":
                        print(f"{ts} - Digit #{idx} (row={r},col={c}) SEG {seg} ON")
                    else:
                        seg_bit = segment_name_to_bit(seg)
                        # build full-grid where only this digit has the seg_bit set
                        grid = [0] * len(digits)
                        grid[idx] = seg_bit
                        ok = apply_grid_to_modules(bus, layout, grid, error_on_fail=False)
                        print(f"{ts} - Digit #{idx} SEG {seg} -> i2c grid write seg=0x{seg_bit:02X} ok={ok}")
                    time.sleep(delay)

            # Phase 2: one digit at a time all segments ON (others OFF)
            for idx, (r, c) in enumerate(digits):
                ts = time.strftime("%Y-%m-%d %H:%M:%S")
                if mode == "print":
                    print(f"{ts} - Digit #{idx} (row={r},col={c}) ALL SEGMENTS ON")
                else:
                    # a..g + dp bits = 0xFF
                    grid = [0] * len(digits)
                    grid[idx] = 0xFF
                    ok = apply_grid_to_modules(bus, layout, grid, error_on_fail=False)
                    print(f"{ts} - Digit #{idx} ALL SEGMENTS ON -> i2c grid write ok={ok}")
                time.sleep(delay)

    except KeyboardInterrupt:
        print("\nInterrupted by user — exiting")
    finally:
        if mode == "i2c" and 'bus' in locals():
            bus.close()


# `main()` をファイル末尾で呼び出します（helper 関数定義の後）


def parse_hex(s: str) -> int:
    if isinstance(s, int):
        return s
    s = s.strip()
    return int(s, 16) if s.startswith("0x") or s.startswith("0X") else int(s)


def collect_ht16_modules(layout: dict) -> list[Tuple[int | None, int | None, int]]:
    """Return list of tuples (tca_addr_or_None, channel_mask_or_None, ht16_addr_int)
    """
    modules: list[Tuple[int | None, int | None, int]] = []
    buses = layout.get("buses", {})
    for bus_k, bus_v in (buses.items() if isinstance(buses, dict) else []):
        tca_list = bus_v.get("tca9548as") if isinstance(bus_v, dict) else None
        if not tca_list:
            continue
        for tca in tca_list:
            tca_addr = tca.get("address")
            tca_addr_int = None if tca_addr is None else parse_hex(tca_addr)
            channels = tca.get("channels", {})
            for ch_key, groups in channels.items():
                ch_mask = None
                try:
                    chi = int(ch_key)
                    if chi >= 0:
                        ch_mask = 1 << chi
                except Exception:
                    ch_mask = None
                # groups is a list of lists of module addresses
                for group in groups:
                    for addr in group:
                        modules.append((tca_addr_int, ch_mask, parse_hex(addr)))
    return modules


def init_modules(bus: SMBus, modules: Iterable[Tuple[int | None, int | None, int]]) -> None:
    for tca_addr, ch_mask, ht_addr in modules:
        if tca_addr is not None and ch_mask is not None:
            try:
                bus.write_byte(tca_addr, ch_mask)
            except Exception:
                pass
        try:
            bus.write_byte(ht_addr, 0x21)
            bus.write_byte(ht_addr, 0x81)
            bus.write_byte(ht_addr, 0xEF)
        except Exception as e:
            print(f"I2C init error on {hex(ht_addr)}: {e}")


def segment_name_to_bit(name: str) -> int:
    m = {"A": 0, "B": 1, "C": 2, "D": 3, "E": 4, "F": 5, "G": 6, "DP": 7}
    i = m.get(name.upper(), 0)
    return 1 << i


def write_segment_to_all(bus: SMBus, modules: Iterable[Tuple[int | None, int | None, int]], seg_bit: int) -> None:
    for tca_addr, ch_mask, ht_addr in modules:
        if tca_addr is not None and ch_mask is not None:
            try:
                bus.write_byte(tca_addr, ch_mask)
            except Exception:
                pass
        try:
            # write seg_bit to all 16 rows (addresses step 2)
            for row in range(16):
                bus.write_byte_data(ht_addr, row * 2, seg_bit)
        except Exception as e:
            print(f"I2C write error on {hex(ht_addr)}: {e}")


def module_size_for_address(layout: dict, addr: int) -> Tuple[int, int]:
    # default module size from layout
    mw = int(layout.get("module_digits_width", 4))
    mh = int(layout.get("module_digits_height", 4))
    # override via module_sizes if present
    mod_sizes = layout.get("module_sizes", {})
    if mod_sizes:
        # keys in JSON may be hex strings
        for k, v in mod_sizes.items():
            try:
                key_addr = parse_hex(k) if isinstance(k, str) else int(k)
            except Exception:
                continue
            if key_addr == addr and isinstance(v, (list, tuple)) and len(v) >= 2:
                try:
                    mw = int(v[0]); mh = int(v[1])
                except Exception:
                    pass
    return mw, mh


def module_index_map_for_address(layout: dict, addr: int):
    m = layout.get("module_index_map", {}) or {}
    for k, v in m.items():
        try:
            key_addr = parse_hex(k) if isinstance(k, str) else int(k)
        except Exception:
            continue
        if key_addr == addr and isinstance(v, list):
            return v
    return None


def module_columns_reversed(layout: dict, addr: int) -> bool:
    rev = layout.get("module_column_reverse", {}) or {}
    for k, v in rev.items():
        try:
            key_addr = parse_hex(k) if isinstance(k, str) else int(k)
        except Exception:
            continue
        if key_addr == addr:
            return bool(v)
    return False


def update_module_from_grid_py(bus: SMBus, ht_addr: int, local_module_buffer: list[int]) -> bool:
    # local_module_buffer: list of per-digit 8-bit masks, length = mw*mh (<=16)
    display_buffer = [0] * 16
    for digit_index in range(min(len(local_module_buffer), 16)):
        bitmask = int(local_module_buffer[digit_index]) & 0xFF
        for seg in range(8):
            if (bitmask >> seg) & 1:
                base = seg * 2
                if digit_index < 8:
                    addr_to_write = base
                    bit_pos = digit_index
                else:
                    addr_to_write = base + 1
                    bit_pos = digit_index - 8
                if addr_to_write < 16:
                    display_buffer[addr_to_write] |= (1 << bit_pos)
    if DEBUG:
        try:
            addr_repr = hex(ht_addr)
        except Exception:
            addr_repr = str(ht_addr)
        print(f"DEBUG: HT {addr_repr} display_buffer: {' '.join(f'{b:02X}' for b in display_buffer)}")
    try:
        bus.write_i2c_block_data(ht_addr, 0x00, display_buffer)
        return True
    except Exception as e:
        print(f"I2C block write error to {hex(ht_addr)}: {e}")
        return False


def apply_grid_to_modules(bus: SMBus, layout: dict, grid: list[int], error_on_fail: bool = False) -> bool:
    # grid is a flat list of length total_width*total_height with 8-bit masks per digit
    total_w = int(layout.get("total_width", 0))
    total_h = int(layout.get("total_height", 0))
    if total_w <= 0 or total_h <= 0:
        print("Invalid layout size for apply_grid")
        return False

    buses = layout.get("buses", {})
    for bus_id, bus_cfg in (buses.items() if isinstance(buses, dict) else []):
        tca_list = bus_cfg.get("tca9548as", [])
        for tca in tca_list:
            use_tca = (tca.get("address") is not None and tca.get("address") != None)
            tca_addr = None
            if use_tca:
                tca_addr = parse_hex(tca.get("address")) if isinstance(tca.get("address"), str) else tca.get("address")
            # rows-based handling
            if tca.get("rows"):
                rows = tca.get("rows", {})
                for row_id, row_cfg in rows.items():
                    channel, row_offset, col_offset = row_cfg
                    address_grid = tca.get("channels", {}).get(channel, [])
                    if use_tca:
                        try:
                            bus.write_byte(tca_addr, 1 << channel)
                        except Exception:
                            pass
                    if not address_grid:
                        continue
                    channel_grid_height = len(address_grid)
                    channel_grid_width = len(address_grid[0])
                    # Heuristic: if channels defined as N rows of single-column
                    # (e.g. [["0x70"],["0x71"],["0x72"]]) but actually
                    # represent a single row of N modules, transpose to treat
                    # them as one row. This matches C implementation expectations.
                    if channel_grid_width == 1 and channel_grid_height > 1:
                        try:
                            cand_width = sum(module_size_for_address(layout, address_grid[r][0])[0] for r in range(channel_grid_height))
                        except Exception:
                            cand_width = 0
                        if cand_width == total_w:
                            # transpose: make a single-row grid with channel_grid_height columns
                            address_grid = [[address_grid[r][0] for r in range(channel_grid_height)]]
                            channel_grid_height = 1
                            channel_grid_width = len(address_grid[0])
                    # compute per-module sizes
                    mod_widths = [[0]*channel_grid_width for _ in range(channel_grid_height)]
                    mod_heights = [[0]*channel_grid_width for _ in range(channel_grid_height)]
                    for rr in range(channel_grid_height):
                        for cc in range(channel_grid_width):
                            maddr = address_grid[rr][cc]
                            mw, mh = module_size_for_address(layout, maddr)
                            mod_widths[rr][cc] = mw
                            mod_heights[rr][cc] = mh

                    for grid_r in range(channel_grid_height):
                        for grid_c in range(channel_grid_width):
                            module_addr_raw = address_grid[grid_r][grid_c]
                            try:
                                module_addr = parse_hex(module_addr_raw) if isinstance(module_addr_raw, str) else int(module_addr_raw)
                            except Exception:
                                module_addr = module_addr_raw
                            col_sum = sum(mod_widths[grid_r][pc] for pc in range(grid_c))
                            module_start_col = col_offset + col_sum
                            row_sum = sum(mod_heights[pr][grid_c] for pr in range(grid_r))
                            module_start_row = row_offset + row_sum
                            mod_w = mod_widths[grid_r][grid_c]
                            mod_h = mod_heights[grid_r][grid_c]
                            local_module_buffer = [0] * (mod_w * mod_h)
                            idx_map = module_index_map_for_address(layout, module_addr)
                            for r_in_mod in range(mod_h):
                                for c_in_mod in range(mod_w):
                                    total_grid_col = module_start_col + c_in_mod
                                    total_grid_row = module_start_row + r_in_mod
                                    grid_index = total_grid_row * total_w + total_grid_col
                                    logical_idx = r_in_mod * mod_w + c_in_mod
                                    module_buffer_index = logical_idx
                                    if idx_map and len(idx_map) == mod_w * mod_h:
                                        module_buffer_index = idx_map[logical_idx]
                                    else:
                                        use_c = c_in_mod
                                        if module_columns_reversed(layout, module_addr):
                                            use_c = (mod_w - 1 - c_in_mod)
                                        module_buffer_index = r_in_mod * mod_w + use_c
                                    if 0 <= grid_index < len(grid) and 0 <= module_buffer_index < len(local_module_buffer):
                                        local_module_buffer[module_buffer_index] = grid[grid_index]
                            if use_tca:
                                try:
                                    bus.write_byte(tca_addr, 1 << channel)
                                except Exception:
                                    pass
                            ok = update_module_from_grid_py(bus, module_addr, local_module_buffer)
                            if not ok and error_on_fail:
                                return False
            else:
                # channel-based handling
                for channel, address_grid in (tca.get("channels", {}).items() if isinstance(tca.get("channels", {}), dict) else []):
                    if use_tca:
                        try:
                            bus.write_byte(tca_addr, 1 << int(channel))
                        except Exception:
                            pass
                    if not address_grid:
                        continue
                    channel_grid_height = len(address_grid)
                    channel_grid_width = len(address_grid[0])
                    # channel offsets special case for 48x8
                    channel_row_offset = 0
                    channel_col_offset = 0
                    if int(layout.get("total_width",0)) == 48 and int(layout.get("total_height",0)) == 8:
                        ch = int(channel)
                        channel_row_offset = (ch % 2) * 4
                        channel_col_offset = (ch // 2) * 24

                    # compute module sizes
                    mod_widths = [[0]*channel_grid_width for _ in range(channel_grid_height)]
                    mod_heights = [[0]*channel_grid_width for _ in range(channel_grid_height)]
                    for rr in range(channel_grid_height):
                        for cc in range(channel_grid_width):
                            maddr = address_grid[rr][cc]
                            mw, mh = module_size_for_address(layout, maddr)
                            mod_widths[rr][cc] = mw
                            mod_heights[rr][cc] = mh

                    bus_col_offset = 0
                    bus_row_offset = 0
                    for grid_r in range(channel_grid_height):
                        for grid_c in range(channel_grid_width):
                            module_addr_raw = address_grid[grid_r][grid_c]
                            try:
                                module_addr = parse_hex(module_addr_raw) if isinstance(module_addr_raw, str) else int(module_addr_raw)
                            except Exception:
                                module_addr = module_addr_raw
                            col_sum = sum(mod_widths[grid_r][pc] for pc in range(grid_c))
                            module_start_col = channel_col_offset + bus_col_offset + col_sum
                            row_sum = sum(mod_heights[pr][grid_c] for pr in range(grid_r))
                            module_start_row = channel_row_offset + bus_row_offset + row_sum
                            mod_w = mod_widths[grid_r][grid_c]
                            mod_h = mod_heights[grid_r][grid_c]
                            local_module_buffer = [0] * (mod_w * mod_h)
                            idx_map = module_index_map_for_address(layout, module_addr)
                            for r_in_mod in range(mod_h):
                                for c_in_mod in range(mod_w):
                                    total_grid_col = module_start_col + c_in_mod
                                    total_grid_row = module_start_row + r_in_mod
                                    grid_index = total_grid_row * total_w + total_grid_col
                                    logical_idx = r_in_mod * mod_w + c_in_mod
                                    module_buffer_index = logical_idx
                                    if idx_map and len(idx_map) == mod_w * mod_h:
                                        module_buffer_index = idx_map[logical_idx]
                                    else:
                                        use_c = c_in_mod
                                        if module_columns_reversed(layout, module_addr):
                                            use_c = (mod_w - 1 - c_in_mod)
                                        module_buffer_index = r_in_mod * mod_w + use_c
                                    if 0 <= grid_index < len(grid) and 0 <= module_buffer_index < len(local_module_buffer):
                                        local_module_buffer[module_buffer_index] = grid[grid_index]
                            if use_tca:
                                try:
                                    bus.write_byte(tca_addr, 1 << int(channel))
                                except Exception:
                                    pass
                            ok = update_module_from_grid_py(bus, module_addr, local_module_buffer)
                            if not ok and error_on_fail:
                                return False
                            # update bus_col_offset similar to C
                            channel_width_in_digits = sum(mod_widths[0][cc] for cc in range(channel_grid_width))
                            channel_height_in_digits = sum(mod_heights[rr][0] for rr in range(channel_grid_height))
                            if (bus_col_offset + channel_width_in_digits) < int(layout.get("total_width",0)):
                                bus_col_offset += channel_width_in_digits
                            else:
                                bus_col_offset = 0
                                bus_row_offset += channel_height_in_digits
    return True


if __name__ == "__main__":
    main()

