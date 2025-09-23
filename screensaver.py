#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import argparse
import time
import random
import curses
import json
from smbus2 import SMBus

# -----------------------------------------------------------------------------
# Gravity / auto-drop timing constants (Moon gravity tweak)
INITIAL_STEP_INTERVAL = 1.5      # seconds per tile at game start
MIN_STEP_INTERVAL     = 0.3      # seconds per tile minimum
STEP_SPEED_DIVISOR    = 5000.0   # divisor used in (INITIAL - score/divisor)
# -----------------------------------------------------------------------------

# ===# ==============================================================================
# ==============================================================================
# スクリーンセーバー定数
# ==============================================================================
DIGITS_PER_MODULE = 16

# 7セグメントディスプレイのセグメントマッピング
# 各文字に対して、点灯するセグメントを1、消灯するセグメントを0で表現
# 例: '0' は a,b,c,d,e,f が点灯し、g は消灯
digit_map={'0':{'a':1,'b':1,'c':1,'d':1,'e':1,'f':1,'g':0},'1':{'a':0,'b':1,'c':1,'d':0,'e':0,'f':0,'g':0},'2':{'a':1,'b':1,'c':0,'d':1,'e':1,'f':0,'g':1},'3':{'a':1,'b':1,'c':1,'d':1,'e':0,'f':0,'g':1},'4':{'a':0,'b':1,'c':1,'d':0,'e':0,'f':1,'g':1},'5':{'a':1,'b':0,'c':1,'d':1,'e':0,'f':1,'g':1},'6':{'a':1,'b':0,'c':1,'d':1,'e':1,'f':1,'g':1},'7':{'a':1,'b':1,'c':1,'d':0,'e':0,'f':0,'g':0},'8':{'a':1,'b':1,'c':1,'d':1,'e':1,'f':1,'g':1},'9':{'a':1,'b':1,'c':1,'d':1,'e':0,'f':1,'g':1},' ': {'a':0,'b':0,'c':0,'d':0,'e':0,'f':0,'g':0},'*':{'a':1,'b':1,'c':1,'d':1,'e':1,'f':1,'g':1},'#':{'a':1,'b':1,'c':1,'d':1,'e':1,'f':1,'g':1},'O':{'a':1,'b':1,'c':1,'d':1,'e':1,'f':1,'g':0},'=':{'a':0,'b':0,'c':0,'d':0,'e':0,'f':0,'g':1},'W':{'a':0,'b':1,'c':1,'d':1,'e':1,'f':0,'g':1},'A':{'a':1,'b':1,'c':1,'d':0,'e':1,'f':1,'g':1},'|':{'a':0,'b':1,'c':1,'d':0,'e':0,'f':0,'g':0},':':{'dp':1},'.':{'dp':1}}
segment_memory_addr={'a':0,'b':2,'c':4,'d':6,'e':8,'f':10,'g':12,'dp':14}
g_current_channel = -2


def load_config(config_name, filename="config.json"):
	"""JSONファイルから指定された設定を読み込み、アドレスを数値に変換する"""
	try:
		with open(filename, 'r') as f:
			all_configs = json.load(f)
		
		if config_name not in all_configs["configurations"]:
			raise KeyError(f"Configuration '{config_name}' not found in {filename}")
			
		config = all_configs["configurations"][config_name]
		
		# 新しいbuses構造に対応
		if "buses" in config:
			# buses構造を古い形式に変換して互換性を保つ
			config["channel_grids"] = {}
			config["tca9548a_address"] = None
			config["bus_number"] = None
			
			for bus_id, bus_config in config["buses"].items():
				config["bus_number"] = int(bus_id)  # 最初のbus IDを使用
				for tca_config in bus_config["tca9548as"]:
					if tca_config.get("address"):
						config["tca9548a_address"] = int(tca_config["address"], 16)
					
					# channelsがある場合
					if "channels" in tca_config:
						for ch_str, grid in tca_config["channels"].items():
							ch_int = int(ch_str)
							new_grid = [[int(addr, 16) for addr in row] for row in grid]
							config["channel_grids"][ch_int] = new_grid
					
					# rowsがある場合（48x8用）
					if "rows" in tca_config:
						# rowsをchannels形式に変換
						channel_modules = {}
						for row_config in tca_config["rows"].values():
							ch = row_config["channel"]
							# 各チャンネルのモジュールを1行として設定
							if str(ch) in tca_config["channels"]:
								# アドレスを数値に変換
								numeric_row = [int(addr, 16) for addr in tca_config["channels"][str(ch)][0]]
								channel_modules[ch] = [numeric_row]  # 1行として設定
						
						for ch, modules in channel_modules.items():
							config["channel_grids"][ch] = modules
						
						# rows情報を保存（C++版と同じ処理のため）
						config["rows"] = tca_config["rows"]						# rows情報を保存（オフセット計算用）
						config["rows"] = tca_config["rows"]		# 古い形式の後方互換性
		else:
			config["bus_number"] = 0  # デフォルト値
			if config.get("tca9548a_address"):
				config["tca9548a_address"] = int(config["tca9548a_address"], 16)
			else:
				config["tca9548a_address"] = None

			new_channel_grids = {}
			for ch_str, grid in config["channel_grids"].items():
				ch_int = int(ch_str)
				new_grid = [[int(addr, 16) for addr in row] for row in grid]
				new_channel_grids[ch_int] = new_grid
			config["channel_grids"] = new_channel_grids
		
		return config

	except FileNotFoundError:
		print(f"Error: Configuration file '{filename}' not found.", file=sys.stderr)
		sys.exit(1)
	except (json.JSONDecodeError, KeyError) as e:
		print(f"Error: Could not parse '{filename}': {e}", file=sys.stderr)
		sys.exit(1)


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
		time.sleep(0.001)  # 1ms待機 (C++版に合わせる)
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
	
	# 48x8でrowsがある場合：C++版と同じrowsベースの処理
	if TOTAL_WIDTH == 48 and config.get("total_height", 0) == 8 and "rows" in config:
		for row_id, row_config in config["rows"].items():
			channel, row_offset, col_offset = row_config["channel"], row_config["row_offset"], row_config["col_offset"]
			
			if channel not in config["channel_grids"]:
				continue
			address_grid = config["channel_grids"][channel]
			
			tca_select_channel(bus, TCA_ADDR, channel)
			if not address_grid:
				continue
			channel_grid_height, channel_grid_width = len(address_grid), len(address_grid[0])
			
			for grid_r, row_of_addrs in enumerate(address_grid):
				for grid_c, module_addr in enumerate(row_of_addrs):
					module_data_buffer = [' '] * digits_per_module
					module_start_col = col_offset + (grid_c * MOD_DIGITS_W)
					module_start_row = row_offset + (grid_r * MOD_DIGITS_H)
					for r_in_mod in range(MOD_DIGITS_H):
						for c_in_mod in range(MOD_DIGITS_W):
							grid_index = (module_start_row + r_in_mod) * TOTAL_WIDTH + (module_start_col + c_in_mod)
							module_buffer_index = r_in_mod * MOD_DIGITS_W + c_in_mod
							if grid_index < len(full_text):
								module_data_buffer[module_buffer_index] = full_text[grid_index]
					update_display_module(bus, module_addr, module_data_buffer)
	else:
		# 従来のチャンネルベースの処理
		sorted_channels = sorted(config["channel_grids"].keys())
		for channel in sorted_channels:
			address_grid = config["channel_grids"][channel]
			tca_select_channel(bus, TCA_ADDR, channel)
			if not address_grid: continue
			channel_grid_height, channel_grid_width = len(address_grid), len(address_grid[0])
			
			# 48x8の場合の特殊処理：チャンネル番号に基づいてオフセットを決定
			channel_row_offset = 0
			channel_col_offset = 0
			if TOTAL_WIDTH == 48 and config.get("total_height", 0) == 8:
				# C++版と同じ計算：チャンネル0,2：上半分、チャンネル1,3：下半分
				channel_row_offset = (int(channel) % 2) * 4
				# チャンネル0,1：左半分、チャンネル2,3：右半分
				channel_col_offset = (int(channel) // 2) * 24
			
			for grid_r, row_of_addrs in enumerate(address_grid):
				for grid_c, module_addr in enumerate(row_of_addrs):
					module_data_buffer = [' '] * digits_per_module
					module_start_col = global_col_offset + channel_col_offset + (grid_c * MOD_DIGITS_W)
					module_start_row = global_row_offset + channel_row_offset + (grid_r * MOD_DIGITS_H)
					for r_in_mod in range(MOD_DIGITS_H):
						for c_in_mod in range(MOD_DIGITS_W):
							grid_index = (module_start_row + r_in_mod) * TOTAL_WIDTH + (module_start_col + c_in_mod)
							module_buffer_index = r_in_mod * MOD_DIGITS_W + c_in_mod
							if grid_index < len(full_text):
								module_data_buffer[module_buffer_index] = full_text[grid_index]
					update_display_module(bus, module_addr, module_data_buffer)
			
			# 48x8の場合は特別なオフセット計算をスキップ
			if TOTAL_WIDTH == 48 and config.get("total_height", 0) == 8:
				continue
				
			channel_width_in_digits = channel_grid_width * MOD_DIGITS_W
			channel_height_in_digits = channel_grid_height * MOD_DIGITS_H
			if (global_col_offset + channel_width_in_digits) < TOTAL_WIDTH:
				global_col_offset += channel_width_in_digits
			else:
				global_col_offset, global_row_offset = 0, global_row_offset + channel_height_in_digits

# ==============================================================================
# スクリーンセーバークラス & ゲームループ
# ==============================================================================
class LEDScreensaver:
	def __init__(self, width, height, fixed_mode=None):
		self.width, self.height = width, height
		if fixed_mode:
			self.modes = [fixed_mode]
			self.mode_duration = float('inf')  # 無限大にして切り替えなし
		else:
			self.modes = ['ip', 'breakout', 'invaders', 'miyajima', 'miyajima2', 'clock']
			self.mode_duration = 10  # 各モードの表示時間（秒）
		self.current_mode = 0
		self.mode_timer = 0
		
		# IP表示用
		import socket
		try:
			s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
			s.connect(("8.8.8.8", 80))
			self.ip_address = s.getsockname()[0]
			s.close()
		except:
			self.ip_address = "127.0.0.1"
		
		# breakout用
		self.breakout_ball = {'x': width//2, 'y': height//2, 'dx': 1, 'dy': 1}
		self.breakout_paddle = {'x': width//2, 'y': height-1, 'width': 5}
		self.breakout_paddle_direction = 1  # パドルの移動方向
		self.breakout_blocks = []
		for y in range(3):
			for x in range(0, width, 2):
				if x + 1 < width:
					self.breakout_blocks.append({'x': x, 'y': y, 'width': 2, 'height': 1})
		
		# invaders用
		self.invaders = []
		for y in range(2):
			for x in range(0, width, 3):
				if x + 2 < width:
					self.invaders.append({'x': x, 'y': y, 'alive': True})
		self.invader_direction = 1
		self.invader_move_timer = 0
		self.invader_move_interval = 2.0
		self.cannon = {'x': width//2, 'y': height-1}
		self.cannon_direction = 1  # 砲台の移動方向
		
		# miyajima用
		self.miyajima_cells = []
		for y in range(height):
			for x in range(width):
				interval = random.randint(1, 255)
				initial_value = random.choice([' '] + [str(i) for i in range(1, 10)])
				self.miyajima_cells.append({'interval': interval, 'value': initial_value, 'timer': 0.0})
		
		# miyajima2用
		self.miyajima2_scroll_timer = 0.0
		self.miyajima2_scroll_interval = 0.5  # 0.5秒ごとにスクロール
		
		# clock用
		self.clock_timer = 0
		self.bullets = []
		self.bullet_timer = 0
		self.bullet_interval = 0.3

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

	def update(self, dt):
		if self.mode_duration != float('inf'):
			self.mode_timer += dt
			if self.mode_timer >= self.mode_duration:
				self.current_mode = (self.current_mode + 1) % len(self.modes)
				self.mode_timer = 0
		
		mode = self.modes[self.current_mode]
		if mode == 'breakout':
			self.update_breakout(dt)
		elif mode == 'invaders':
			self.update_invaders(dt)
		elif mode == 'miyajima':
			self.update_miyajima(dt)
		elif mode == 'miyajima2':
			self.update_miyajima2(dt)
		elif mode == 'clock':
			self.update_clock(dt)
	
	def update_breakout(self, dt):
		# パドルの移動 - ボールを追う
		ball_x = self.breakout_ball['x']
		paddle_x = self.breakout_paddle['x']
		if ball_x < paddle_x - 1:
			self.breakout_paddle['x'] -= 0.3  # 左に移動
		elif ball_x > paddle_x + 1:
			self.breakout_paddle['x'] += 0.3  # 右に移動
		
		# パドルの境界チェック
		if self.breakout_paddle['x'] < self.breakout_paddle['width']//2:
			self.breakout_paddle['x'] = self.breakout_paddle['width']//2
		if self.breakout_paddle['x'] > self.width - self.breakout_paddle['width']//2:
			self.breakout_paddle['x'] = self.width - self.breakout_paddle['width']//2
		
		# ボールの移動 (速度をさらに遅く)
		self.breakout_ball['x'] += self.breakout_ball['dx'] * 0.3
		self.breakout_ball['y'] += self.breakout_ball['dy'] * 0.3
		
		# 壁との衝突
		if self.breakout_ball['x'] <= 0 or self.breakout_ball['x'] >= self.width - 1:
			self.breakout_ball['dx'] *= -1
		if self.breakout_ball['y'] <= 0:
			self.breakout_ball['dy'] *= -1
		
		# パドルとの衝突
		if (self.breakout_ball['y'] >= self.breakout_paddle['y'] and 
			self.breakout_paddle['x'] - self.breakout_paddle['width']//2 <= self.breakout_ball['x'] <= self.breakout_paddle['x'] + self.breakout_paddle['width']//2):
			self.breakout_ball['dy'] *= -1
		
		# ブロックとの衝突
		for block in self.breakout_blocks[:]:
			if (block['x'] <= self.breakout_ball['x'] < block['x'] + block['width'] and
				block['y'] <= self.breakout_ball['y'] < block['y'] + block['height']):
				self.breakout_blocks.remove(block)
				self.breakout_ball['dy'] *= -1
				break
		
		# 全てのブロックを消したらリセット
		if not self.breakout_blocks:
			self.breakout_ball = {'x': self.width//2, 'y': self.height//2, 'dx': 1, 'dy': 1}
			self.breakout_blocks = []
			for y in range(3):
				for x in range(0, self.width, 2):
					if x + 1 < self.width:
						self.breakout_blocks.append({'x': x, 'y': y, 'width': 2, 'height': 1})
		
		# ボールが下に落ちたらリセット
		if self.breakout_ball['y'] >= self.height:
			self.breakout_ball = {'x': self.width//2, 'y': self.height//2, 'dx': 1, 'dy': 1}
	
	def update_invaders(self, dt):
		# 砲台の移動
		self.cannon['x'] += self.cannon_direction * 0.3
		if self.cannon['x'] >= self.width - 1:
			self.cannon['x'] = self.width - 1
			self.cannon_direction = -1
		if self.cannon['x'] <= 0:
			self.cannon['x'] = 0
			self.cannon_direction = 1
		
		self.invader_move_timer += dt
		if self.invader_move_timer >= self.invader_move_interval:
			self.invader_move_timer = 0
			# インベーダーの移動
			for invader in self.invaders:
				if invader['alive']:
					invader['x'] += self.invader_direction
			
			# 端に到達したら方向転換
			if self.invaders:
				min_x = min(inv['x'] for inv in self.invaders if inv['alive'])
				max_x = max(inv['x'] + 2 for inv in self.invaders if inv['alive'])
				if min_x <= 0 or max_x >= self.width:
					self.invader_direction *= -1
					for invader in self.invaders:
						if invader['alive']:
							invader['y'] += 1
							# 砲台との衝突判定
							if invader['y'] >= self.height - 1:
								# 敵が砲台に到達したらリセット
								self.invaders = []
								for y in range(2):
									for x in range(0, self.width, 3):
										if x + 2 < self.width:
											self.invaders.append({'x': x, 'y': y, 'alive': True})
								self.invader_direction = 1
								break
		
		# 弾の発射
		self.bullet_timer += dt
		if self.bullet_timer >= self.bullet_interval:
			self.bullet_timer = 0
			self.bullets.append({'x': self.cannon['x'], 'y': self.cannon['y'] - 1})
		
		# 弾の移動
		for bullet in self.bullets[:]:
			bullet['y'] -= 1
			if bullet['y'] < 0:
				self.bullets.remove(bullet)
			else:
				# インベーダーとの衝突判定
				for invader in self.invaders:
					if (invader['alive'] and 
						invader['x'] <= bullet['x'] < invader['x'] + 3 and
						invader['y'] <= bullet['y'] < invader['y'] + 1):
						invader['alive'] = False
						if bullet in self.bullets:
							self.bullets.remove(bullet)
						break
		
		# 全てのインベーダーが死んだらリセット
		if not any(invader['alive'] for invader in self.invaders):
			self.invaders = []
			for y in range(2):
				for x in range(0, self.width, 3):
					if x + 2 < self.width:
						self.invaders.append({'x': x, 'y': y, 'alive': True})
			self.invader_direction = 1

	def update_miyajima(self, dt):
		for cell in self.miyajima_cells:
			cell['timer'] += dt
			while cell['timer'] >= cell['interval'] * 0.1:  # interval × 100ms
				cell['timer'] -= cell['interval'] * 0.1
				# カウントアップ (9 → 空白 → 1 → 2 → ... → 9)
				if cell['value'] == ' ':
					cell['value'] = '1'
				elif cell['value'] == '9':
					cell['value'] = ' '
				else:
					cell['value'] = str(int(cell['value']) + 1)
	
	def update_miyajima2(self, dt):
		# カウントアップ
		for cell in self.miyajima_cells:
			cell['timer'] += dt
			while cell['timer'] >= cell['interval'] * 0.1:  # interval × 100ms
				cell['timer'] -= cell['interval'] * 0.1
				# カウントアップ (9 → 空白 → 1 → 2 → ... → 9)
				if cell['value'] == ' ':
					cell['value'] = '1'
				elif cell['value'] == '9':
					cell['value'] = ' '
				else:
					cell['value'] = str(int(cell['value']) + 1)
		
		# スクロール
		self.miyajima2_scroll_timer += dt
		if self.miyajima2_scroll_timer >= self.miyajima2_scroll_interval:
			self.miyajima2_scroll_timer = 0.0
			# 右にシフト
			for y in range(self.height):
				# 各行の最後のデータを保存
				last_value = self.miyajima_cells[y * self.width + (self.width - 1)]['value']
				last_interval = self.miyajima_cells[y * self.width + (self.width - 1)]['interval']
				# 右にシフト
				for x in range(self.width - 1, 0, -1):
					idx = y * self.width + x
					prev_idx = y * self.width + (x - 1)
					self.miyajima_cells[idx]['value'] = self.miyajima_cells[prev_idx]['value']
					self.miyajima_cells[idx]['interval'] = self.miyajima_cells[prev_idx]['interval']
				# 最初の位置に最後のデータを入れる
				self.miyajima_cells[y * self.width]['value'] = last_value
				self.miyajima_cells[y * self.width]['interval'] = last_interval
	
	def update_clock(self, dt):
		self.clock_timer += dt
		# 時計は毎秒更新
	
	def get_render_string(self):
		field = [[' '] * self.width for _ in range(self.height)]
		mode = self.modes[self.current_mode]
		
		if mode == 'ip':
			# IPアドレスをセンタリングして表示（ドットをDPに）
			ip_str = self.ip_address.replace('.', ':')
			start_x = max(0, (self.width - len(ip_str)) // 2)
			start_y = self.height // 2
			for i, char in enumerate(ip_str):
				if start_x + i < self.width:
					field[start_y][start_x + i] = char
		
		elif mode == 'breakout':
			# ボール
			x, y = int(self.breakout_ball['x']), int(self.breakout_ball['y'])
			if 0 <= x < self.width and 0 <= y < self.height:
				field[y][x] = 'O'
			
			# パドル
			paddle_y = int(self.breakout_paddle['y'])
			paddle_x = int(self.breakout_paddle['x'])
			paddle_start = paddle_x - self.breakout_paddle['width']//2
			paddle_end = paddle_start + self.breakout_paddle['width']
			for x in range(max(0, paddle_start), min(self.width, paddle_end)):
				field[paddle_y][x] = '='
			
			# ブロック
			for block in self.breakout_blocks:
				for dy in range(block['height']):
					for dx in range(block['width']):
						y, x = int(block['y']) + dy, int(block['x']) + dx
						if 0 <= x < self.width and 0 <= y < self.height:
							field[y][x] = '#'
		
		elif mode == 'invaders':
			# インベーダー
			for invader in self.invaders:
				if invader['alive']:
					for dx in range(3):
						x = int(invader['x']) + dx
						y = int(invader['y'])
						if 0 <= x < self.width and 0 <= y < self.height:
							field[y][x] = 'W'
			
			# 砲台
			cannon_x, cannon_y = int(self.cannon['x']), int(self.cannon['y'])
			if 0 <= cannon_x < self.width and 0 <= cannon_y < self.height:
				field[cannon_y][cannon_x] = 'A'
			
			# 弾
			for bullet in self.bullets:
				x, y = int(bullet['x']), int(bullet['y'])
				if 0 <= x < self.width and 0 <= y < self.height:
					field[y][x] = '|'
		
		elif mode == 'miyajima':
			# miyajimaパターン
			for i, cell in enumerate(self.miyajima_cells):
				y, x = divmod(i, self.width)
				field[y][x] = cell['value']
		
		elif mode == 'miyajima2':
			# miyajima2パターン (スクロール)
			for i, cell in enumerate(self.miyajima_cells):
				y, x = divmod(i, self.width)
				field[y][x] = cell['value']
		
		return "".join(["".join(row) for row in field])

def play_screensaver_session(stdscr, bus, config, args):
		curses.curs_set(0)
		DISPLAY_WIDTH, DISPLAY_HEIGHT = config["total_width"], config["total_height"]
		screensaver = LEDScreensaver(DISPLAY_WIDTH, DISPLAY_HEIGHT, args.mode)
		
		RENDER_INTERVAL = 1.0 / 15  # 1秒間に15回画面を更新
		last_render_time = 0
		last_update_time = time.time()
		 
		stdscr.nodelay(1)  # キー入力を待たない非ブロッキングモード
		while True:
			current_time = time.time()
			dt = current_time - last_update_time
			last_update_time = current_time
			
			# キー操作
			key = stdscr.getch()
			if key != -1:
				if key == ord('q'):
					return
			
			# スクリーンセーバーの更新
			screensaver.update(dt)
			
			# 画面描画
			if current_time - last_render_time > RENDER_INTERVAL:
				screensaver_field_str = screensaver.get_render_string()
				
				# Curses画面（ターミナル）の描画
				stdscr.clear()
				mode_name = screensaver.modes[screensaver.current_mode].upper()
				stdscr.addstr(0, 0, f"MODE: {mode_name}")
				for y in range(DISPLAY_HEIGHT):
					line = screensaver_field_str[y * DISPLAY_WIDTH : (y + 1) * DISPLAY_WIDTH]
					stdscr.addstr(y + 2, 0, line)
				stdscr.refresh()
				
				# LEDパネルの描画
				update_flexible_display(bus, config, screensaver_field_str)
				last_render_time = current_time
				
			time.sleep(0.01)

# ==============================================================================
# メイン実行部
# ==============================================================================
def main():
	parser = argparse.ArgumentParser(description="LED Screensaver for 7-segment LED display.")
	parser.add_argument('--config', type=str, default='16x12', 
						help='Display configuration name defined in config.json (e.g., 16x12, 12x8).')
	parser.add_argument('--test', action='store_true', 
						help='Run test display instead of screensaver.')
	parser.add_argument('--mode', type=str, choices=['ip', 'breakout', 'invaders', 'miyajima', 'miyajima2', 'clock'], 
						help='Fixed mode to display (default: cycle through all modes).')
	args = parser.parse_args()

	# ★★★ JSONファイルから設定を読み込む ★★★
	active_config = load_config(args.config)

	bus = None
	try:
		bus_number = active_config.get("bus_number", 0)
		bus = SMBus(bus_number) 
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
						time.sleep(0.001)  # 各モジュール初期化後に待機
					except IOError as e:
						print(f"Failed to initialize CH{channel} addr {hex(addr)}: {e}", file=sys.stderr)
						return
		
		if TCA_ADDR is not None:
			tca_select_channel(bus, TCA_ADDR, -1)
		
		if args.test:
			# テスト表示: 012345... を48文字表示
			test_text = "".join(str(i % 10) for i in range(48))  # 01234567890123456789012345678901234567890123456789
			print(f"Test text: {test_text}")
			update_flexible_display(bus, active_config, test_text)
			time.sleep(5)  # 5秒表示
		else:
			curses.wrapper(play_screensaver_session, bus, active_config, args)

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



