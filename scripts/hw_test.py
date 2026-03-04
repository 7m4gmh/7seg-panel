#!/usr/bin/env python3
"""
Hardware I2C scan and simple HT16K33 display test.

Run on the target (Raspberry Pi) with I2C enabled.

Examples:
  pip3 install smbus2
  python3 scripts/hw_test.py --scan
  python3 scripts/hw_test.py --test 0x70 0x71
"""
import argparse
from smbus2 import SMBus, i2c_msg


def scan(busnum=1):
    found = []
    with SMBus(busnum) as bus:
        for addr in range(0x03, 0x78):
            try:
                # quick write to test presence
                bus.write_quick(addr)
                found.append(hex(addr))
            except Exception:
                pass
    return found


def init_ht16k33(bus, addr):
    # Turn on oscillator
    bus.write_byte(addr, 0x21)
    # Display on, no blinking
    bus.write_byte(addr, 0x81)
    # Full brightness (0-15)
    bus.write_byte(addr, 0xE0 | 15)


def fill_display_all(bus, addr):
    # 16 bytes of display RAM starting at address 0x00
    data = [0xFF] * 16
    bus.write_i2c_block_data(addr, 0x00, data)


def clear_display(bus, addr):
    data = [0x00] * 16
    bus.write_i2c_block_data(addr, 0x00, data)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--bus', type=int, default=1)
    p.add_argument('--scan', action='store_true')
    p.add_argument('--test', nargs='+', help='Addresses to test (e.g. 0x70 0x71)')
    args = p.parse_args()

    if args.scan:
        print('Scanning I2C bus', args.bus)
        found = scan(args.bus)
        print('Found addresses:', ' '.join(found) if found else '(none)')
        return

    if args.test:
        addrs = [int(x, 16) for x in args.test]
        with SMBus(args.bus) as bus:
            for a in addrs:
                try:
                    print(f'Init {hex(a)}')
                    init_ht16k33(bus, a)
                    print(f'Fill display {hex(a)}')
                    fill_display_all(bus, a)
                except Exception as e:
                    print(f'Error with {hex(a)}: {e}')
        return

    p.print_help()


if __name__ == '__main__':
    main()
