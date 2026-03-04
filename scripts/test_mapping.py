#!/usr/bin/env python3
"""
Simple mapping tester for panel configurations.

Usage:
  python3 scripts/test_mapping.py --name 8x2-direct --cycle
  python3 scripts/test_mapping.py --name 8x2-direct --swap 0x70 --cycle

This reads `config.json`, prints logical -> physical mapping,
and can simulate cycling a single lit column to help debug wiring/order.
"""
import json
import argparse
from pathlib import Path


def load_config(path):
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)


def find_direct_channels(cfg, name):
    confs = cfg.get('configurations', {})
    conf = confs.get(name)
    if not conf:
        raise SystemExit(f"Configuration '{name}' not found in config.json")
    buses = conf.get('buses', {})
    # pick first bus and first tca9548a entry
    for bus in buses.values():
        tcas = bus.get('tca9548as') or bus.get('tca9548as')
        if tcas:
            tca = tcas[0]
            channels = tca.get('channels')
            if channels and '-1' in channels:
                return conf, channels['-1']
    raise SystemExit('Direct channel (-1) not found for this configuration')


def build_mapping(conf, blocks):
    total_w = conf['total_width']
    total_h = conf['total_height']
    m_w = conf['module_digits_width']
    m_h = conf['module_digits_height']

    # blocks: list of module-rows, each is list of module addresses across columns
    # number of module columns = len(blocks[0])
    mapping = {}  # (r,c) -> (addr, local_r, local_c)
    for r in range(total_h):
        module_row = r // m_h
        local_r = r % m_h
        for c in range(total_w):
            module_col = c // m_w
            local_c = c % m_w
            try:
                addr = blocks[module_row][module_col]
            except IndexError:
                addr = None
            mapping[(r, c)] = (addr, local_r, local_c)
    return mapping


def print_mapping(conf, blocks, mapping):
    total_w = conf['total_width']
    total_h = conf['total_height']
    print(f"Configuration: {conf.get('name')}")
    print(f"size: {total_w}x{total_h}, module: {conf['module_digits_width']}x{conf['module_digits_height']}")
    print('\nLogical grid -> (addr, local_r, local_c)')
    for r in range(total_h):
        row_elems = []
        for c in range(total_w):
            addr, lr, lc = mapping[(r, c)]
            row_elems.append(f"{addr or 'None'}:{lr},{lc}")
        print(' | '.join(row_elems))


def simulate_cycle(conf, blocks, mapping, swap_addrs=None):
    total_w = conf['total_width']
    total_h = conf['total_height']
    m_w = conf['module_digits_width']

    for col in range(total_w):
        grid = [["." for _ in range(total_w)] for __ in range(total_h)]
        for r in range(total_h):
            grid[r][col] = "#"
        print(f"\n-- Column {col} --")
        for r in range(total_h):
            print(''.join(grid[r]))
        # show which physical addresses/local cols are driven for this logical column
        addrs = set()
        for r in range(total_h):
            addr, lr, lc = mapping[(r, col)]
            if swap_addrs and addr in swap_addrs:
                lc = m_w - 1 - lc
            addrs.add((addr, lr, lc))
        print('Physical targets for this column:')
        for a in sorted(addrs):
            print(f"  addr={a[0]}, local_r={a[1]}, local_c={a[2]}")


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--config', default='config.json')
    p.add_argument('--name', default='8x2-direct')
    p.add_argument('--cycle', action='store_true', help='Cycle columns and print mapping')
    p.add_argument('--swap', nargs='*', help='List of module addresses to reverse column order for (e.g. 0x70)')
    args = p.parse_args()

    cfg_path = Path(args.config)
    if not cfg_path.exists():
        # try repo root path
        cfg_path = Path(__file__).resolve().parents[1] / 'config.json'
    cfg = load_config(cfg_path)
    conf, blocks = find_direct_channels(cfg, args.name)
    mapping = build_mapping(conf, blocks)
    print_mapping(conf, blocks, mapping)
    if args.cycle:
        simulate_cycle(conf, blocks, mapping, swap_addrs=set(args.swap or []))


if __name__ == '__main__':
    main()
