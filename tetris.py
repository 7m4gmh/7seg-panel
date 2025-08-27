#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import argparse
import time
import random
import curses
from smbus2 import SMBus

# --- 構成を柔軟に変更できるようにリファクタリングしたコード ---

# ==============================================================================
# ▼▼▼ パネル構成定義 ▼▼▼
# 使用したい構成のコメントを外してください。
# ==============================================================================

# --- 元の構成: 24桁 x 4行 (4x4モジュールを横に6個並べたもの) ---
CONFIG_24x4 = {
    "name": "24x4 Horizontal",
    "module_grid": [ # モジュールの物理的な配置 (行x列)
        [0x70, 0x71, 0x72, 0x73, 0x74, 0x75]
    ],
    "module_digits_width": 4,  # 1モジュールあたりの桁数 (横)
    "module_digits_height": 4, # 1モジュールあたりの桁数 (縦)
    "total_width": 24,         # 全体の表示幅
    "total_height": 4          # 全体の表示高さ
}

# --- 拡張構成の例: 12桁 x 8行 (4x4モジュールを3x2のグリッド状に配置) ---
CONFIG_12x8 = {
    "name": "12x8 Grid",
    "module_grid": [ # 3モジュールを2段に配置
        [0x70, 0x71, 0x72],
        [0x73, 0x74, 0x75]
    ],
    "module_digits_width": 4,
    "module_digits_height": 4,
    "total_width": 12, # 3モジュール * 4桁
    "total_height": 8  # 2段 * 4行
}

# ==============================================================================
# グローバル定数 (変更不要)
# ==============================================================================
DIGITS_PER_MODULE=16 # 1モジュールあたりのLED数 (4x4=16)
digit_map={'0':{'a':1,'b':1,'c':1,'d':1,'e':1,'f':1,'g':0},'1':{'a':0,'b':1,'c':1,'d':0,'e':0,'f':0,'g':0},'2':{'a':1,'b':1,'c':0,'d':1,'e':1,'f':0,'g':1},'3':{'a':1,'b':1,'c':1,'d':1,'e':0,'f':0,'g':1},'4':{'a':0,'b':1,'c':1,'d':0,'e':0,'f':1,'g':1},'5':{'a':1,'b':0,'c':1,'d':1,'e':0,'f':1,'g':1},'6':{'a':1,'b':0,'c':1,'d':1,'e':1,'f':1,'g':1},'7':{'a':1,'b':1,'c':1,'d':0,'e':0,'f':0,'g':0},'8':{'a':1,'b':1,'c':1,'d':1,'e':1,'f':1,'g':1},'9':{'a':1,'b':1,'c':1,'d':1,'e':0,'f':1,'g':1},'A':{'a':1,'b':1,'c':1,'d':0,'e':1,'f':1,'g':1},'B':{'a':0,'b':0,'c':1,'d':1,'e':1,'f':1,'g':1},'C':{'a':1,'b':0,'c':0,'d':1,'e':1,'f':1,'g':0},'D':{'a':0,'b':1,'c':1,'d':1,'e':1,'f':0,'g':1},'E':{'a':1,'b':0,'c':0,'d':1,'e':1,'f':1,'g':1},'F':{'a':1,'b':0,'c':0,'d':0,'e':1,'f':1,'g':1},'G':{'a':1,'b':0,'c':1,'d':1,'e':1,'f':1,'g':0},'H':{'a':0,'b':1,'c':1,'d':0,'e':1,'f':1,'g':1},'I':{'a':0,'b':0,'c':0,'d':0,'e':1,'f':1,'g':0},'J':{'a':0,'b':1,'c':1,'d':1,'e':0,'f':0,'g':0},'K':{'a':0,'b':1,'c':1,'d':0,'e':1,'f':1,'g':1},'L':{'a':0,'b':0,'c':0,'d':1,'e':1,'f':1,'g':0},'M':{'a':1,'b':1,'c':1,'d':0,'e':1,'f':1,'g':0},'N':{'a':0,'b':0,'c':1,'d':0,'e':1,'f':0,'g':1},'O':{'a':1,'b':1,'c':1,'d':1,'e':1,'f':1,'g':0},'P':{'a':1,'b':1,'c':0,'d':0,'e':1,'f':1,'g':1},'Q':{'a':1,'b':1,'c':1,'d':0,'e':0,'f':1,'g':1},'R':{'a':0,'b':0,'c':0,'d':0,'e':1,'f':0,'g':1},'S':{'a':1,'b':0,'c':1,'d':1,'e':0,'f':1,'g':1},'T':{'a':0,'b':0,'c':0,'d':1,'e':1,'f':1,'g':1},'U':{'a':0,'b':1,'c':1,'d':1,'e':1,'f':1,'g':0},'V':{'a':0,'b':1,'c':1,'d':1,'e':1,'f':0,'g':0},'W':{'a':0,'b':1,'c':1,'d':1,'e':1,'f':1,'g':1},'X':{'a':0,'b':1,'c':1,'d':0,'e':1,'f':1,'g':1},'Y':{'a':0,'b':1,'c':1,'d':1,'e':0,'f':1,'g':1},'Z':{'a':1,'b':1,'c':0,'d':1,'e':1,'f':0,'g':1},' ': {'a':0,'b':0,'c':0,'d':0,'e':0,'f':0,'g':0},'-':{'g':1},'_':{'d':1},'.':{'dp':1},'*':{'a':1,'b':1,'c':1,'d':1,'e':1,'f':1,'g':1},'#':{'b':1,'c':1,'d':1,'e':1,'f':1,'g':1},'=':{'d':1,'g':1},"'":{'f':1},"|":{'b':1,'c':1}}
segment_memory_addr={'a':0,'b':2,'c':4,'d':6,'e':8,'f':10,'g':12,'dp':14}
SHAPES={'I':[[(0,0),(1,0)],[(0,0),(0,1)]],'O':[[(0,0),(1,0),(0,1),(1,1)]],'T':[[(0,0),(-1,0),(1,0),(0,1)],[(0,0),(0,-1),(0,1),(1,0)],[(0,0),(-1,0),(1,0),(0,-1)],[(0,0),(0,-1),(0,1),(-1,0)]],'L':[[(0,0),(1,0),(-1,0),(1,1)],[(0,0),(0,1),(0,-1),(-1,1)],[(0,0),(1,0),(-1,0),(-1,-1)],[(0,0),(0,1),(0,-1),(1,-1)]],'S':[[(0,0),(1,0),(0,1),(-1,1)],[(0,0),(0,-1),(1,0),(1,1)]]}
SHAPE_KEYS=list(SHAPES.keys())

# ==============================================================================
# 低レベルI2C通信関数 (変更なし)
# ==============================================================================
def update_display(bus, ht16k33_addr, text):
    display_buffer = [0] * 16
    for digit_index, char_to_display in enumerate(text[:DIGITS_PER_MODULE]):
        char_key = char_to_display if char_to_display in digit_map else ' '
        for seg_name, seg_on in digit_map[char_key].items():
            if seg_on:
                addr, bit_pos = (segment_memory_addr.get(seg_name), digit_index) if digit_index < 8 else (segment_memory_addr.get(seg_name)+1, digit_index-8)
                if addr is not None: display_buffer[addr] |= (1 << bit_pos)
    try:
        bus.write_i2c_block_data(ht16k33_addr, 0x00, display_buffer)
    except IOError as e:
        print(f"[{time.time():.2f}] I2C Write Error on Address {hex(ht16k33_addr)}: {e}", file=sys.stderr)

# ==============================================================================
# 座標マッピングと描画実行関数 (リファクタリングの核)
# ==============================================================================
def update_flexible_display(bus, config, full_text):
    """
    柔軟な構成に対応したディスプレイ更新関数
    configオブジェクトに基づいて、1次元の文字列を各モジュールのデータにマッピングする
    """
    MODULE_GRID = config["module_grid"]
    MOD_DIGITS_W = config["module_digits_width"]
    MOD_DIGITS_H = config["module_digits_height"]
    TOTAL_WIDTH = config["total_width"]
    TOTAL_HEIGHT = config["total_height"]
    TOTAL_DIGITS = TOTAL_WIDTH * TOTAL_HEIGHT

    all_addresses = [addr for row in MODULE_GRID for addr in row]
    module_chars = {addr: [' '] * DIGITS_PER_MODULE for addr in all_addresses}

    for i, char in enumerate(full_text[:TOTAL_DIGITS]):
        total_row, total_col = i // TOTAL_WIDTH, i % TOTAL_WIDTH
        
        module_grid_row = total_row // MOD_DIGITS_H
        module_grid_col = total_col // MOD_DIGITS_W
        
        try:
            addr = MODULE_GRID[module_grid_row][module_grid_col]
        except IndexError:
            continue
            
        row_in_module = total_row % MOD_DIGITS_H
        col_in_module = total_col % MOD_DIGITS_W
        
        digit_index = (row_in_module * MOD_DIGITS_W) + col_in_module
        
        module_chars[addr][digit_index] = char

    for addr, chars in module_chars.items():
        update_display(bus, addr, "".join(chars))

# ==============================================================================
# アプリケーションクラス (変更なし)
# ==============================================================================
class DemoMode:
    def __init__(self,width,height,density=0.2):self.width,self.height,self.density,self.grid,self.raindrops=width,height,density,[[' ']*width for _ in range(height)],['|', '.', "'"]
    def tick(self):
        for r in range(self.height-1,0,-1):self.grid[r]=self.grid[r-1][:]
        for c in range(self.width):self.grid[0][c]=random.choice(self.raindrops) if random.random()<self.density else ' '
        return "".join(["".join(r) for r in self.grid])

class SideTetrisGame:
    def __init__(self,width,height):
        self.width,self.height=width,height;self.field=[[' ']*width for _ in range(height)];self.score,self.game_over=0,False;self.score_width=6;self.new_block()
    def new_block(self):
        self.current_shape_key=random.choice(SHAPE_KEYS);self.current_shape=SHAPES[self.current_shape_key];self.rotation=0;self.block_pos={'y':1,'x':self.score_width}
        if self.check_collision(self.get_current_rotation(),self.block_pos):self.game_over=True
    def get_current_rotation(self):return self.current_shape[self.rotation%len(self.current_shape)]
    def check_collision(self,shape,pos):
        for dy,dx in shape:
            ny,nx=pos['y']+dy,pos['x']+dx
            if not(0<=nx<self.width and 0<=ny<self.height)or self.field[ny][nx]!=' ':return True
        return False
    def move(self,dy):
        n={'y':self.block_pos['y']+dy,'x':self.block_pos['x']}
        if not self.check_collision(self.get_current_rotation(),n):self.block_pos=n
    def lock_block(self):
        for dy,dx in self.get_current_rotation():
            y,x=self.block_pos['y']+dy,self.block_pos['x']+dx
            if 0<=y<self.height and 0<=x<self.width:self.field[y][x]='*'
    def clear_lines(self):
        new_field_columns=[]
        for x in range(self.width):
            if not all(self.field[y][x]!=' ' for y in range(self.height)):new_field_columns.append([self.field[y][x] for y in range(self.height)])
        lines_cleared=self.width-len(new_field_columns)
        if lines_cleared==0:return
        final_columns=[[' ']*self.height for _ in range(lines_cleared)]+new_field_columns
        for y in range(self.height):
            for x in range(self.width):self.field[y][x]=final_columns[x][y]
        self.score+=[0,100,300,500,800][lines_cleared]
    def rotate(self):
        nr=(self.rotation+1)%len(self.current_shape);
        if not self.check_collision(self.current_shape[nr],self.block_pos):self.rotation=nr
    def step_right(self):
        n={'y':self.block_pos['y'],'x':self.block_pos['x']+1}
        if not self.check_collision(self.get_current_rotation(),n):self.block_pos=n
        else:self.lock_block();self.clear_lines();self.new_block()
    def get_render_string(self):
        t=[r[:] for r in self.field]
        for dy,dx in self.get_current_rotation():
            y,x=self.block_pos['y']+dy,self.block_pos['x']+dx
            if 0<=y<self.height and 0<=x<self.width:t[y][x]='#'
        for i,c in enumerate(str(self.score).ljust(self.score_width)):
            if i<self.width:t[0][i]=c
        return "".join(["".join(r) for r in t])

# ==============================================================================
# ゲームロジック (configオブジェクトを受け取るように修正)
# ==============================================================================
def play_tetris_game(stdscr, bus, config):
    DISPLAY_WIDTH = config["total_width"]
    DISPLAY_HEIGHT = config["total_height"]
    game = SideTetrisGame(DISPLAY_WIDTH, DISPLAY_HEIGHT)
    
    RENDER_FPS = 15; RENDER_INTERVAL = 1.0 / RENDER_FPS
    last_render_time, last_step_time, step_interval = 0, time.time(), 0.5
    stdscr.nodelay(1)
    while not game.game_over:
        try:
            key = stdscr.getch()
            if key != -1:
                if key == curses.KEY_UP: game.move(-1)
                elif key == curses.KEY_DOWN: game.move(1)
                elif key == ord(' '): game.rotate()
                elif key == curses.KEY_RIGHT: game.step_right(); last_step_time = time.time()
                elif key == ord('q'): return
            current_time = time.time()
            if current_time - last_step_time > step_interval:
                game.step_right(); last_step_time = current_time
            if current_time - last_render_time > RENDER_INTERVAL:
                update_flexible_display(bus, config, game.get_render_string())
                last_render_time = current_time
            time.sleep(0.01)
        except Exception as e:
            print(f"Unhandled error in game loop: {e}", file=sys.stderr)
            break
    
    game_over_text = ("="*DISPLAY_WIDTH + "GAME OVER".center(DISPLAY_WIDTH) + 
                      f"SCORE {game.score}".center(DISPLAY_WIDTH) + 
                      "PRESS ANY KEY".center(DISPLAY_WIDTH))
    update_flexible_display(bus, config, game_over_text)
    stdscr.nodelay(0); stdscr.getch()

def run_demo_mode(stdscr, bus, config):
    demo = DemoMode(config["total_width"], config["total_height"])
    stdscr.nodelay(1)
    while True:
        if stdscr.getch() != -1: return
        update_flexible_display(bus, config, demo.tick())
        time.sleep(0.15)

def game_session(stdscr, bus, config):
    curses.curs_set(0)
    DISPLAY_WIDTH = config["total_width"]
    demo_timeout=10.0
    while True:
        welcome_text = ("SIDEWAYS TETRIS".center(DISPLAY_WIDTH) + 
                        (' '*DISPLAY_WIDTH) + 
                        "PRESS ANY KEY TO START".center(DISPLAY_WIDTH) + 
                        "      PRESS Q TO QUIT".center(DISPLAY_WIDTH))
        update_flexible_display(bus, config, welcome_text)
        
        stdscr.nodelay(1); start_time=time.time(); key_pressed=-1
        while time.time()-start_time < demo_timeout:
            key=stdscr.getch()
            if key!=-1: key_pressed=key; break
            time.sleep(0.05)
        
        if key_pressed==ord('q'): break
        elif key_pressed!=-1: play_tetris_game(stdscr, bus, config)
        else: run_demo_mode(stdscr, bus, config)

# ==============================================================================
# メイン実行部 (configオブジェクトを生成・利用するように修正)
# ==============================================================================
def main():
    parser = argparse.ArgumentParser(description="Flexible 7-segment LED display control.")
    parser.add_argument('--game', type=str, default='tetris', help='Game to run (e.g., "tetris").')
    parser.add_argument('--config', type=str, default='24x4', choices=['24x4', '12x8'], help='Display configuration to use.')
    args = parser.parse_args()

    # 引数に基づいて使用する設定を選択
    if args.config == '12x8':
        active_config = CONFIG_12x8
    else:
        active_config = CONFIG_24x4

    bus = None
    try:
        if args.game == 'tetris':
            bus = SMBus(0) # ROCK5は0, RPiは1
            print(f"Using configuration: {active_config['name']}")
            print("Initializing modules...")
            
            # 2次元のgridから全アドレスをフラットなリストとして取得
            all_module_addresses = [addr for row in active_config["module_grid"] for addr in row]

            for addr in all_module_addresses:
                try:
                    bus.write_byte(addr, 0x21) # System setup: oscillator on
                    bus.write_byte(addr, 0x81) # Display setup: display on, no blink
                    bus.write_byte(addr, 0xEF) # Dimming setup: max brightness
                except IOError as e:
                    print(f"Failed to initialize address {hex(addr)}: {e}", file=sys.stderr)
                    return
            
            # game_sessionにconfigオブジェクトを渡す
            curses.wrapper(game_session, bus, active_config)
        else:
            print("Run with --game tetris to play.")

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
        print("Cleanup complete. Exiting.", file=sys.stderr)

if __name__ == '__main__':
    main()


