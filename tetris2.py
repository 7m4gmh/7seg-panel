#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import argparse
import time
import random
import curses
from smbus2 import SMBus

# ==============================================================================
# ▼▼▼ パネル構成定義 ▼▼▼
# ==============================================================================
CONFIG_24x4 = {
    "name": "24x4 Horizontal",
    "tca9548a_address": 0x77,
    "channel_grids": {0: [[0x70, 0x71, 0x72, 0x73, 0x74, 0x75]]},
    "module_digits_width": 4, "module_digits_height": 4,
    "total_width": 24, "total_height": 4
}
CONFIG_12x8_EXPANDED = {
    "name": "12x8 Expanded (2x 12x4 via TCA9548a)",
    "tca9548a_address": 0x77,
    "channel_grids": {0: [[0x70, 0x71, 0x72]], 1: [[0x70, 0x71, 0x72]]},
    "module_digits_width": 4, "module_digits_height": 4,
    "total_width": 12, "total_height": 8
}
CONFIG_48x4_EXPANDED = {
    "name": "48x4 Expanded (2x 24x4 via TCA9548a)",
    "tca9548a_address": 0x77,
    "channel_grids": {0: [[0x70, 0x71, 0x72, 0x73, 0x74, 0x75]], 1: [[0x70, 0x71, 0x72, 0x73, 0x74, 0x75]]},
    "module_digits_width": 4, "module_digits_height": 4,
    "total_width": 48, "total_height": 4
}
CONFIG_12x8_DIRECT = {
    "name": "12x8 Grid (Direct Connection)",
    "tca9548a_address": None,
    "channel_grids": {-1: [[0x70, 0x71, 0x72], [0x73, 0x74, 0x75]]},
    "module_digits_width": 4, "module_digits_height": 4,
    "total_width": 12, "total_height": 8
}

# ==============================================================================
# ▼▼▼ ゲーム定数（縦落ちテトリス用に刷新） ▼▼▼
# ==============================================================================
SHAPES = {
    'I': [[(0, -1), (0, 0), (0, 1), (0, 2)], [(-1, 0), (0, 0), (1, 0), (2, 0)]],
    'O': [[(0, 0), (1, 0), (0, 1), (1, 1)]],
    'T': [[(-1, 0), (0, 0), (1, 0), (0, -1)], [(0, -1), (0, 0), (0, 1), (1, 0)], [(-1, 0), (0, 0), (1, 0), (0, 1)], [(0, -1), (0, 0), (0, 1), (-1, 0)]],
    'L': [[(0, -1), (0, 0), (0, 1), (1, 1)], [(-1, 0), (0, 0), (1, 0), (1, -1)], [(-1, -1), (0, -1), (0, 0), (0, 1)], [(-1, 1), (-1, 0), (0, 0), (1, 0)]],
    'J': [[(0, -1), (0, 0), (0, 1), (-1, 1)], [(-1, -1), (-1, 0), (0, 0), (1, 0)], [(1, -1), (0, -1), (0, 0), (0, 1)], [(-1, 0), (0, 0), (1, 0), (1, 1)]],
    'S': [[(-1, 0), (0, 0), (0, -1), (1, -1)], [(0, -1), (0, 0), (1, 0), (1, 1)]],
    'Z': [[(-1, -1), (0, -1), (0, 0), (1, 0)], [(1, -1), (1, 0), (0, 0), (0, 1)]]
}
SHAPE_KEYS = list(SHAPES.keys())
DIGITS_PER_MODULE = 16
digit_map={'0':{'a':1,'b':1,'c':1,'d':1,'e':1,'f':1,'g':0},'1':{'a':0,'b':1,'c':1,'d':0,'e':0,'f':0,'g':0},'2':{'a':1,'b':1,'c':0,'d':1,'e':1,'f':0,'g':1},'3':{'a':1,'b':1,'c':1,'d':1,'e':0,'f':0,'g':1},'4':{'a':0,'b':1,'c':1,'d':0,'e':0,'f':1,'g':1},'5':{'a':1,'b':0,'c':1,'d':1,'e':0,'f':1,'g':1},'6':{'a':1,'b':0,'c':1,'d':1,'e':1,'f':1,'g':1},'7':{'a':1,'b':1,'c':1,'d':0,'e':0,'f':0,'g':0},'8':{'a':1,'b':1,'c':1,'d':1,'e':1,'f':1,'g':1},'9':{'a':1,'b':1,'c':1,'d':1,'e':0,'f':1,'g':1},' ': {'a':0,'b':0,'c':0,'d':0,'e':0,'f':0,'g':0},'*':{'a':1,'b':1,'c':1,'d':1,'e':1,'f':1,'g':1},'#':{'a':1,'b':1,'c':1,'d':1,'e':1,'f':1,'g':1}}
segment_memory_addr={'a':0,'b':2,'c':4,'d':6,'e':8,'f':10,'g':12,'dp':14}
g_current_channel = -2

# ==============================================================================
# 低レベルI2C通信 & 描画関数
# ==============================================================================
def tca_select_channel(bus, tca_addr, channel):
    global g_current_channel
    if tca_addr is None:
        g_current_channel = -1
        return
    if channel == g_current_channel: return
    try:
        control_byte = 1 << channel if channel >= 0 else 0
        bus.write_byte(tca_addr, control_byte)
        g_current_channel = channel
    except IOError as e:
        print(f"Failed to select TCA channel {channel} on addr {hex(tca_addr)}: {e}", file=sys.stderr)

def update_display_module(bus, ht16k33_addr, text_list):
    display_buffer = [0] * 16
    for digit_index, char_to_display in enumerate(text_list[:DIGITS_PER_MODULE]):
        char_key = char_to_display if char_to_display in digit_map else ' '
        for seg_name, seg_on in digit_map[char_key].items():
            if seg_on:
                addr, bit_pos = (segment_memory_addr.get(seg_name), digit_index) if digit_index < 8 else (segment_memory_addr.get(seg_name)+1, digit_index-8)
                if addr is not None: display_buffer[addr] |= (1 << bit_pos)
    try:
        bus.write_i2c_block_data(ht16k33_addr, 0x00, display_buffer)
    except IOError as e:
        print(f"I2C Write Error on Address {hex(ht16k33_addr)}: {e}", file=sys.stderr)

def update_flexible_display(bus, config, full_text):
    TCA_ADDR = config.get("tca9548a_address")
    MOD_DIGITS_W, MOD_DIGITS_H = config["module_digits_width"], config["module_digits_height"]
    TOTAL_WIDTH = config["total_width"]
    digits_per_module = MOD_DIGITS_W * MOD_DIGITS_H
    global_row_offset, global_col_offset = 0, 0
    sorted_channels = sorted(config["channel_grids"].keys())
    for channel in sorted_channels:
        address_grid = config["channel_grids"][channel]
        tca_select_channel(bus, TCA_ADDR, channel)
        if not address_grid: continue
        channel_grid_height, channel_grid_width = len(address_grid), len(address_grid[0])
        for grid_r, row_of_addrs in enumerate(address_grid):
            for grid_c, module_addr in enumerate(row_of_addrs):
                module_data_buffer = [' '] * digits_per_module
                module_start_col = global_col_offset + (grid_c * MOD_DIGITS_W)
                module_start_row = global_row_offset + (grid_r * MOD_DIGITS_H)
                for r_in_mod in range(MOD_DIGITS_H):
                    for c_in_mod in range(MOD_DIGITS_W):
                        grid_index = (module_start_row + r_in_mod) * TOTAL_WIDTH + (module_start_col + c_in_mod)
                        module_buffer_index = r_in_mod * MOD_DIGITS_W + c_in_mod
                        if grid_index < len(full_text):
                            module_data_buffer[module_buffer_index] = full_text[grid_index]
                update_display_module(bus, module_addr, module_data_buffer)
        channel_width_in_digits = channel_grid_width * MOD_DIGITS_W
        channel_height_in_digits = channel_grid_height * MOD_DIGITS_H
        if (global_col_offset + channel_width_in_digits) < TOTAL_WIDTH:
            global_col_offset += channel_width_in_digits
        else:
            global_col_offset, global_row_offset = 0, global_row_offset + channel_height_in_digits

# ==============================================================================
# 縦落ちテトリスのゲームクラス & ゲームループ
# ==============================================================================
class VerticalTetrisGame:
    def __init__(self, width, height):
        self.width, self.height = width, height
        self.field = [[' '] * width for _ in range(height)]
        self.score, self.game_over = 0, False
        self.new_block()

    def new_block(self):
        self.current_shape_key = random.choice(SHAPE_KEYS)
        self.current_shape = SHAPES[self.current_shape_key]
        self.rotation = 0
        shape = self.get_current_rotation()
        min_dy = min(dy for dy, dx in shape)
        self.block_pos = {'y': 0 - min_dy, 'x': self.width // 2}
        if self.check_collision(shape, self.block_pos): self.game_over = True

    def get_current_rotation(self): return self.current_shape[self.rotation % len(self.current_shape)]

    def check_collision(self, shape, pos):
        for dy, dx in shape:
            ny, nx = pos['y'] + dy, pos['x'] + dx
            if not (0 <= nx < self.width and 0 <= ny < self.height) or self.field[ny][nx] != ' ':
                return True
        return False

    def move_horizontal(self, dx):
        new_pos = {'y': self.block_pos['y'], 'x': self.block_pos['x'] + dx}
        if not self.check_collision(self.get_current_rotation(), new_pos): self.block_pos = new_pos

    def move_down(self):
        new_pos = {'y': self.block_pos['y'] + 1, 'x': self.block_pos['x']}
        if not self.check_collision(self.get_current_rotation(), new_pos):
            self.block_pos = new_pos
            return True
        return False

    def rotate(self):
        next_rotation = (self.rotation + 1) % len(self.current_shape)
        if not self.check_collision(self.current_shape[next_rotation], self.block_pos):
            self.rotation = next_rotation

    def lock_block(self):
        for dy, dx in self.get_current_rotation():
            y, x = self.block_pos['y'] + dy, self.block_pos['x'] + dx
            if 0 <= y < self.height and 0 <= x < self.width: self.field[y][x] = '*'

    def clear_lines(self):
        new_field, lines_cleared = [], 0
        for row in self.field:
            if ' ' in row: new_field.append(row)
            else: lines_cleared += 1
        empty_rows = [[' '] * self.width for _ in range(lines_cleared)]
        self.field = empty_rows + new_field
        if lines_cleared > 0: self.score += [0, 100, 300, 500, 800][lines_cleared]

    def step(self):
        if not self.move_down():
            self.lock_block()
            self.clear_lines()
            self.new_block()

    def get_render_string(self):
        temp_field = [row[:] for row in self.field]
        for dy, dx in self.get_current_rotation():
            y, x = self.block_pos['y'] + dy, self.block_pos['x'] + dx
            if 0 <= y < self.height and 0 <= x < self.width: temp_field[y][x] = '#'
        return "".join(["".join(row) for row in temp_field])

def play_game_session(stdscr, bus, config):
        """ ★★★ この関数を修正 ★★★ """
        curses.curs_set(0)
        DISPLAY_WIDTH, DISPLAY_HEIGHT = config["total_width"], config["total_height"]
        game = VerticalTetrisGame(DISPLAY_WIDTH, DISPLAY_HEIGHT)
        
        RENDER_INTERVAL = 1.0 / 15  # 1秒間に15回画面を更新
        last_render_time = 0
        step_interval = 0.5  # ブロックが自動で1マス落ちる時間
        last_step_time = time.time()
         
        stdscr.nodelay(1)  # キー入力を待たない非ブロッキングモード
        while not game.game_over:
            current_time = time.time()
            
            # --- ▼▼▼ キー操作のロジックを修正 ▼▼▼ ---
            key = stdscr.getch()
            if key != -1:
                if key == curses.KEY_LEFT:
                    game.move_horizontal(-1)
                elif key == curses.KEY_RIGHT:
                    game.move_horizontal(1)
                elif key == curses.KEY_DOWN:
                    # 1マス下に移動し、自動落下のタイマーをリセット
                    if game.move_down():
                        last_step_time = current_time
                elif key == curses.KEY_UP or key == ord(' '):  # ↑キーまたはスペースキー
                    game.rotate()
                elif key == ord('q'):
                    return
            # --- ▲▲▲ ここまで修正 ▲▲▲ ---

            # 自動落下（重力）
            if current_time - last_step_time > step_interval:
                game.step()
                last_step_time = current_time
                # スコアに応じてゲーム速度を上げる
                step_interval = max(0.1, 0.5 - game.score / 10000.0)

            # 画面描画
            if current_time - last_render_time > RENDER_INTERVAL:
                game_field_str = game.get_render_string()
                
                # Curses画面（ターミナル）の描画
                stdscr.clear()
                stdscr.addstr(0, 0, f"SCORE: {game.score}")
                for y in range(DISPLAY_HEIGHT):
                    line = game_field_str[y * DISPLAY_WIDTH : (y + 1) * DISPLAY_WIDTH]
                    stdscr.addstr(y + 2, 0, line)
                stdscr.refresh()
                
                # LEDパネルの描画
                update_flexible_display(bus, config, game_field_str)
                last_render_time = current_time
                
            time.sleep(0.01)
        
        # ゲームオーバー処理
        game_over_text = ("GAME OVER".center(DISPLAY_WIDTH) + (' ' * DISPLAY_WIDTH) + 
                          f"SCORE {game.score}".center(DISPLAY_WIDTH) + "PRESS ANY KEY".center(DISPLAY_WIDTH))
        update_flexible_display(bus, config, game_over_text)
        stdscr.nodelay(0)
        stdscr.getch()

# ==============================================================================
# メイン実行部
# ==============================================================================
def main():
    parser = argparse.ArgumentParser(description="Vertical Tetris for 7-segment LED display.")
    parser.add_argument('--config', type=str, default='12x8', 
                        choices=['24x4', '12x8', '48x4', '12x8-direct'], 
                        help='Display configuration to use.')
    args = parser.parse_args()

    config_map = {
        '12x8': CONFIG_12x8_EXPANDED,
        '48x4': CONFIG_48x4_EXPANDED,
        '12x8-direct': CONFIG_12x8_DIRECT,
        '24x4': CONFIG_24x4
    }
    active_config = config_map.get(args.config, CONFIG_24x4)

    bus = None
    try:
        bus = SMBus(0) # ROCK5は0, RPiは1
        print(f"Using configuration: {active_config['name']}")
        print("Initializing modules...")
        
        TCA_ADDR = active_config.get("tca9548a_address")
        for channel, address_grid in active_config["channel_grids"].items():
            tca_select_channel(bus, TCA_ADDR, channel)
            time.sleep(0.01)
            for row in address_grid:
                for addr in row:
                    try:
                        bus.write_byte_data(addr, 0x21, 0) # System setup: oscillator on
                        bus.write_byte_data(addr, 0x81, 0) # Display setup: display on, no blink
                        bus.write_byte_data(addr, 0xEF, 0) # Dimming setup: max brightness
                    except IOError as e:
                        print(f"Failed to initialize CH{channel} addr {hex(addr)}: {e}", file=sys.stderr)
                        return
        
        if TCA_ADDR is not None:
            tca_select_channel(bus, TCA_ADDR, -1)
        
        curses.wrapper(play_game_session, bus, active_config)

    except FileNotFoundError:
        print(f"Error: I2C bus device not found. Check your bus number (e.g., /dev/i2c-0).", file=sys.stderr)
    except (KeyboardInterrupt, SystemExit):
        print("\nExiting gracefully.", file=sys.stderr)
    except Exception as e:
        print(f"A fatal error occurred: {e}", file=sys.stderr)
        try: curses.endwin()
        except: pass
    finally:
        if bus:
            print("Cleaning up display...", file=sys.stderr)
            try:
                total_digits = active_config["total_width"] * active_config["total_height"]
                empty_screen = " " * total_digits
                update_flexible_display(bus, active_config, empty_screen)
                bus.close()
            except Exception as e:
                print(f"Could not clear displays on exit: {e}", file=sys.stderr)
        
        try:
            if curses.isendwin() is False:
                curses.endwin()
        except:
            pass
        print("Cleanup complete. Exiting.", file=sys.stderr)

if __name__ == '__main__':
    main()



