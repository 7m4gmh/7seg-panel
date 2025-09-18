# ----------------------------------------
# プロジェクト設定
# ----------------------------------------
PROJECT_NAME = 7seg-panel
VERSION = 1.0.0


# --- リンク（emulator_test専用） ---
$(EMULATOR_TEST_BIN): $(OBJDIR)/emulator_test.o $(OBJDIR)/emulator_display.o
	@echo "Linking $@..."
	$(CXX) -o $@ $^ $(BASE_LDFLAGS) $(CV_SDL_LIBS)
	@echo "Successfully built -> $@"

$(UDP_PLAYER_BIN): $(OBJS_COMMON) $(UDP_PLAYER_OBJS)
	@echo "Linking $@..."
	$(CXX) -o $@ $^ $(BASE_LDFLAGS) $(CV_SDL_LIBS)
	@echo "Successfully built -> $@"

$(FILE_PLAYER_BIN): $(OBJS_COMMON) $(FILE_PLAYER_OBJS)
	@echo "Linking $@..."
	$(CXX) -o $@ $^ $(BASE_LDFLAGS) $(CV_SDL_LIBS)
	@echo "Successfully built -> $@"

$(HTTP_PLAYER_BIN): $(OBJS_COMMON) $(HTTP_PLAYER_OBJS)
	@echo "Linking $@..."
	$(CXX) -o $@ $^ $(BASE_LDFLAGS) $(CV_SDL_LIBS)
	@echo "Successfully built -> $@"  = 1.0.0

# OS検出
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
IS_RASPBERRY_PI := $(shell grep -q "Raspberry Pi" /proc/device-tree/model 2>/dev/null && echo "yes" || echo "no")

# アーキテクチャディレクトリ決定
ifeq ($(UNAME_S),Darwin)
    ifeq ($(UNAME_M),arm64)
        ARCH_DIR := darwin-arm64
    else
        ARCH_DIR := darwin-amd64
    endif
else ifeq ($(UNAME_S),Linux)
    ifeq ($(UNAME_M),aarch64)
        ifeq ($(IS_RASPBERRY_PI),yes)
            ARCH_DIR := linux-arm64-rpi5
        else
            ARCH_DIR := linux-arm64-rock5b
        endif
    else ifeq ($(UNAME_M),x86_64)
        ARCH_DIR := linux-amd64
    else
        ARCH_DIR := linux-$(UNAME_M)
    endif
else
    ARCH_DIR := $(UNAME_S)-$(UNAME_M)
endif

# バイナリ出力ディレクトリ
BINDIR := bin/$(ARCH_DIR)

NUM_CORES := $(shell nproc 2>/dev/null || echo 4)
MAKEFLAGS += -j$(NUM_CORES)

# デフォルトはヘルプを表示（何もビルドしない）
.DEFAULT_GOAL := help

# ----------------------------------------
# コンパイラと共通設定
# ----------------------------------------
CXX = g++
INCLUDES = -Iinclude -I../cpp-httplib
BASE_CXXFLAGS = -std=c++17 -Wall -O2 $(INCLUDES)
ifeq ($(UNAME_S),Darwin)
    BASE_CXXFLAGS += -D__APPLE__
    BASE_LDFLAGS = 
else
    BASE_LDFLAGS = -pthread
endif

# ----------------------------------------
# 依存ライブラリごとの設定
# ----------------------------------------
ifeq ($(IS_RASPBERRY_PI),yes)
    # Raspberry Pi 5
    CV_SDL_CFLAGS = -I/usr/include/SDL2 -I/usr/include/opencv4
    CV_SDL_LIBS = -lSDL2 -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs
else ifeq ($(UNAME_S),Darwin)
    # macOS
    CV_SDL_CFLAGS = -I/opt/homebrew/include/SDL2 -I/opt/homebrew/include/opencv4
    CV_SDL_LIBS = -L/opt/homebrew/lib -lSDL2 -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs
else
    # ROCK5B (Debian-based Linux, no Homebrew)
    CV_SDL_CFLAGS = -I/usr/include/SDL2 -I/usr/include/opencv4
    CV_SDL_LIBS = -lSDL2 -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs
endif
# GStreamer: core + app (appsink/src) + video (gst/video/video.h)
# pkg-config のモジュール名は distro により同じ: gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0
GST_CFLAGS = $(shell pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
GST_LIBS   = $(shell pkg-config --libs   gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)

# ----------------------------------------
# ソースファイルとオブジェクトファイル
# ----------------------------------------
SRCDIR = src
OBJDIR = obj
DEPDIR = $(OBJDIR)

SRCS_COMMON        = $(wildcard $(SRCDIR)/common.cpp $(SRCDIR)/led.cpp $(SRCDIR)/video.cpp $(SRCDIR)/audio.cpp $(SRCDIR)/playback.cpp)
SRCS_COMMON       += $(SRCDIR)/file_audio_stub.cpp
UDP_PLAYER_SRCS    = $(wildcard $(SRCDIR)/udp_player.cpp $(SRCDIR)/udp.cpp)
FILE_PLAYER_SRCS   = $(wildcard $(SRCDIR)/file_player.cpp)
ifeq ($(UNAME_S),Linux)
    FILE_PLAYER_SRCS  += $(SRCDIR)/file_audio_gst.cpp
else
    FILE_PLAYER_SRCS  += $(SRCDIR)/file_audio_stub.cpp
endif
HTTP_PLAYER_SRCS   = $(wildcard $(SRCDIR)/http_player.cpp)
RTP_PLAYER_SRCS    = $(wildcard $(SRCDIR)/rtp_player.cpp)
NET_PLAYER_SRCS    = $(wildcard $(SRCDIR)/net_player.cpp)
TEST_I2C_SRCS      = $(SRCDIR)/test_i2c.cpp

OBJS_COMMON         = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS_COMMON))
UDP_PLAYER_OBJS     = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(UDP_PLAYER_SRCS))
FILE_PLAYER_OBJS    = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(FILE_PLAYER_SRCS))
HTTP_PLAYER_OBJS    = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(HTTP_PLAYER_SRCS))
RTP_PLAYER_OBJS     = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(RTP_PLAYER_SRCS))
NET_PLAYER_OBJS     = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(NET_PLAYER_SRCS))
TEST_I2C_OBJS       = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(TEST_I2C_SRCS))

ALL_OBJS = $(OBJS_COMMON) $(UDP_PLAYER_OBJS) $(FILE_PLAYER_OBJS) $(HTTP_PLAYER_OBJS) $(RTP_PLAYER_OBJS) $(NET_PLAYER_OBJS) $(TEST_I2C_OBJS)
DEPS     = $(patsubst $(OBJDIR)/%.o,$(DEPDIR)/%.d,$(ALL_OBJS))

# ----------------------------------------
# 実行ファイル名
# ----------------------------------------
UDP_PLAYER_BIN    = $(BINDIR)/7seg-udp-player
FILE_PLAYER_BIN   = $(BINDIR)/7seg-file-player
HTTP_PLAYER_BIN   = $(BINDIR)/7seg-http-player
EMULATOR_TEST_BIN = $(BINDIR)/emulator_test
RTP_PLAYER_BIN    = $(BINDIR)/7seg-rtp-player
NET_PLAYER_BIN    = $(BINDIR)/7seg-net-player
TEST_I2C_BIN      = $(BINDIR)/7seg-test-i2c

# ターゲットのグループ
CORE_TARGETS = $(UDP_PLAYER_BIN) $(FILE_PLAYER_BIN) $(HTTP_PLAYER_BIN)
GST_TARGETS  = $(RTP_PLAYER_BIN) $(NET_PLAYER_BIN)
TARGETS      = $(CORE_TARGETS) $(GST_TARGETS) $(TEST_I2C_BIN)

# RTP/NETプレイヤー用の専用フラグ
$(RTP_PLAYER_OBJS) $(NET_PLAYER_OBJS): CXXFLAGS = $(BASE_CXXFLAGS) $(CV_SDL_CFLAGS) $(GST_CFLAGS)

# ----------------------------------------
# ターゲットごとのフラグ
# ----------------------------------------
OBJS_CV_SDL = $(OBJS_COMMON) $(UDP_PLAYER_OBJS) $(FILE_PLAYER_OBJS) $(HTTP_PLAYER_OBJS)
$(OBJS_CV_SDL): CXXFLAGS = $(BASE_CXXFLAGS) $(CV_SDL_CFLAGS)

$(TEST_I2C_OBJS): CXXFLAGS = $(BASE_CXXFLAGS) $(CV_SDL_CFLAGS)

# file_audio_gst は GStreamer ヘッダが必要
$(OBJDIR)/file_audio_gst.o: CXXFLAGS = $(BASE_CXXFLAGS) $(CV_SDL_CFLAGS) $(GST_CFLAGS)

# ----------------------------------------
# ビルドルール
# ----------------------------------------
.PHONY: all core gst rtp net clean package deb help emulator_test emulator

# すべて（core + gst）
all: $(BINDIR) $(TARGETS)

$(BINDIR):
	mkdir -p $(BINDIR)

# GStreamer非依存のプレイヤーのみ
core: $(BINDIR) $(CORE_TARGETS)

# GStreamer依存ターゲットのみ
gst: $(BINDIR) $(GST_TARGETS)

# 個別ターゲット
rtp: $(BINDIR) $(RTP_PLAYER_BIN)
net: $(BINDIR) $(NET_PLAYER_BIN)
file: $(BINDIR) $(FILE_PLAYER_BIN)
http: $(BINDIR) $(HTTP_PLAYER_BIN)
test: $(BINDIR) $(TEST_I2C_BIN)

# --- 実行ファイルのリンク ---
$(UDP_PLAYER_BIN): $(OBJS_COMMON) $(UDP_PLAYER_OBJS)
	@echo "Linking $@..."
	$(CXX) -o $@ $^ $(BASE_LDFLAGS) $(CV_SDL_LIBS)
	@echo "Successfully built -> $@"

$(FILE_PLAYER_BIN): $(OBJS_COMMON) $(FILE_PLAYER_OBJS)
	@echo "Linking $@..."
	$(CXX) -o $@ $^ $(BASE_LDFLAGS) $(CV_SDL_LIBS) $(GST_LIBS)
	@echo "Successfully built -> $@"


# --- オブジェクトファイルのコンパイル（emulator_test専用） ---
$(OBJDIR)/emulator_test.o: $(SRCDIR)/emulator_test.cpp | $(DEPDIR)
	@echo "Compiling $<..."
	$(CXX) $(BASE_CXXFLAGS) $(CV_SDL_CFLAGS) -MMD -MP -c $< -o $@

$(HTTP_PLAYER_BIN): $(OBJS_COMMON) $(HTTP_PLAYER_OBJS)
	@echo "Linking $@..."
	$(CXX) -o $@ $^ $(BASE_LDFLAGS) $(CV_SDL_LIBS)
	@echo "Successfully built -> $@"


# --- リンク（emulator_test専用） ---
$(EMULATOR_TEST_BIN): $(OBJDIR)/emulator_test.o $(OBJDIR)/emulator_display.o
	@echo "Linking $@..."
	$(CXX) -o $@ $^ $(BASE_LDFLAGS) $(CV_SDL_LIBS)
	@echo "Successfully built -> $@"

$(NET_PLAYER_BIN): $(OBJS_COMMON) $(NET_PLAYER_OBJS)
	@echo "Linking $@..."
	$(CXX) -o $@ $^ $(BASE_LDFLAGS) $(CV_SDL_LIBS) $(GST_LIBS)
	@echo "Successfully built -> $@"

$(RTP_PLAYER_BIN): $(OBJS_COMMON) $(RTP_PLAYER_OBJS)
	@echo "Linking $@..."
	$(CXX) -o $@ $^ $(BASE_LDFLAGS) $(CV_SDL_LIBS) $(GST_LIBS)
	@echo "Successfully built -> $@"

$(TEST_I2C_BIN): $(OBJDIR)/common.o $(OBJDIR)/led.o $(OBJDIR)/video.o $(OBJDIR)/test_i2c.o
	@echo "Linking $@..."
	$(CXX) -o $@ $^ $(BASE_LDFLAGS) $(CV_SDL_LIBS)
	@echo "Successfully built -> $@"

# --- オブジェクトファイルのコンパイル ---
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(DEPDIR)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(DEPDIR):
	@mkdir -p $(DEPDIR)

# 依存関係ファイルをインクルード
-include $(DEPS)

# ----------------------------------------
# パッケージ作成 (ソース)
# ----------------------------------------
PACKAGE       = $(PROJECT_NAME)-$(VERSION)
ARCHIVE       = $(PACKAGE).tar.gz
PACKAGE_FILES = src include www Makefile README.md README.en.md README.ja.md README.zh-CN.md docs

package: $(ARCHIVE)

$(ARCHIVE):
	@echo "Creating source package: $(ARCHIVE)..."
	rm -rf $(PACKAGE)
	mkdir -p $(PACKAGE)
	cp -r $(PACKAGE_FILES) $(PACKAGE)/
	cp -r ../cpp-httplib $(PACKAGE)/
	sed -i 's|-I../cpp-httplib|-I./cpp-httplib|g' $(PACKAGE)/Makefile
	tar -czf $(ARCHIVE) $(PACKAGE)
	rm -rf $(PACKAGE)
	@echo "Source package created successfully: $(ARCHIVE)"

# ----------------------------------------
# Debian パッケージ作成 (バイナリ)
# ----------------------------------------
ARCH     := $(shell dpkg --print-architecture)
DEB_NAME := $(PROJECT_NAME)_$(VERSION)_$(ARCH).deb
DEB_DIR  := deb_build

deb: $(DEB_NAME)

$(DEB_NAME): all
	@echo "Creating Debian package: $(DEB_NAME)..."
	rm -rf $(DEB_DIR)
	mkdir -p $(DEB_DIR)/DEBIAN
	mkdir -p $(DEB_DIR)/usr/local/bin
	mkdir -p $(DEB_DIR)/usr/local/share/$(PROJECT_NAME)/www
	mkdir -p $(DEB_DIR)/usr/local/share/$(PROJECT_NAME)/default_videos
	@echo "Package: $(PROJECT_NAME)" > $(DEB_DIR)/DEBIAN/control
	@echo "Version: $(VERSION)" >> $(DEB_DIR)/DEBIAN/control
	@echo "Architecture: $(ARCH)" >> $(DEB_DIR)/DEBIAN/control
	@echo "Maintainer: Your Name <your.email@example.com>" >> $(DEB_DIR)/DEBIAN/control
	@echo "Depends: libopencv-core, libopencv-videoio, libsdl2-2.0-0, libgstreamer1.0-0" >> $(DEB_DIR)/DEBIAN/control
	@echo "Description: 7-Segment LED Panel Video Player" >> $(DEB_DIR)/DEBIAN/control
	@echo " A suite of tools to play videos on a custom I2C 7-segment display." >> $(DEB_DIR)/DEBIAN/control
	cp $(TARGETS) $(DEB_DIR)/usr/local/bin/
	cp -r www/* $(DEB_DIR)/usr/local/share/$(PROJECT_NAME)/www/
	if [ -d "default_videos" ] && [ "$(ls -A default_videos)" ]; then cp -r default_videos/* $(DEB_DIR)/usr/local/share/$(PROJECT_NAME)/default_videos/; fi
	dpkg-deb --build $(DEB_DIR)
	mv $(DEB_DIR).deb $(DEB_NAME)
	rm -rf $(DEB_DIR)
	@echo "Debian package created successfully: $(DEB_NAME)"

# ----------------------------------------
# クリーンアップ
# ----------------------------------------
clean:
	rm -rf $(OBJDIR) $(TARGETS) emulator_test $(PROJECT_NAME)-*.tar.gz

# ----------------------------------------
# ヘルプ
# ----------------------------------------


help:
	@echo ""
	@echo "Usage:"
	@echo "  make core       - Build non-GStreamer players: $(CORE_TARGETS)"
	@echo "  make gst        - Build GStreamer targets:    $(GST_TARGETS)"
	@echo "  make rtp        - Build only:                 $(RTP_PLAYER_BIN)"
	@echo "  make test       - Build only:                 $(TEST_I2C_BIN)"
	@echo "  make all        - Build all targets"
	@echo "  make clean      - Remove build artifacts"
	@echo "  make package    - Create source tarball"
	@echo "  make deb        - Create Debian package"
	@echo ""

emulator: obj/emulator_test.o obj/emulator_display.o
	$(CXX) -o emulator_test obj/emulator_test.o obj/emulator_display.o $(BASE_LDFLAGS) $(CV_SDL_LIBS)
	@echo "Successfully linked -> emulator_test"

benchmark: obj/emulator_benchmark.o obj/emulator_display.o
	$(CXX) -o emulator_benchmark obj/emulator_benchmark.o obj/emulator_display.o $(BASE_LDFLAGS) $(CV_SDL_LIBS)
	@echo "Successfully linked -> emulator_benchmark"

$(OBJDIR)/emulator_display.o: $(SRCDIR)/emulator_display.cpp | $(DEPDIR)
	@echo "Compiling $<..."
	$(CXX) $(BASE_CXXFLAGS) $(CV_SDL_CFLAGS) -MMD -MP -c $< -o $@
