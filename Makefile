# ----------------------------------------
# プロジェクト設定
# ----------------------------------------
PROJECT_NAME = 7seg-panel
VERSION      = 1.0.0

NUM_CORES := $(shell nproc 2>/dev/null || echo 4)
MAKEFLAGS += -j$(NUM_CORES)

# ----------------------------------------
# コンパイラと共通設定
# ----------------------------------------
CXX = g++
INCLUDES = -Iinclude -I../cpp-httplib
BASE_CXXFLAGS = -std=c++17 -Wall -O2 $(INCLUDES)
BASE_LDFLAGS = -pthread

# ----------------------------------------
# 依存ライブラリごとの設定
# ----------------------------------------
CV_SDL_CFLAGS = $(shell pkg-config --cflags opencv4)
CV_SDL_LIBS   = -lSDL2 $(shell pkg-config --libs opencv4)
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

SRCS_COMMON = $(wildcard $(SRCDIR)/common.cpp $(SRCDIR)/led.cpp $(SRCDIR)/video.cpp $(SRCDIR)/audio.cpp $(SRCDIR)/playback.cpp)
UDP_PLAYER_SRCS = $(wildcard $(SRCDIR)/udp_player.cpp $(SRCDIR)/udp.cpp)
FILE_PLAYER_SRCS = $(wildcard $(SRCDIR)/file_player.cpp)
HTTP_PLAYER_SRCS = $(wildcard $(SRCDIR)/http_player.cpp)
SRCS_HTTP_STREAMER = $(wildcard $(SRCDIR)/http_streamer.cpp)

OBJS_COMMON = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS_COMMON))
UDP_PLAYER_OBJS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(UDP_PLAYER_SRCS))
FILE_PLAYER_OBJS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(FILE_PLAYER_SRCS))
HTTP_PLAYER_OBJS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(HTTP_PLAYER_SRCS))
OBJS_HTTP_STREAMER = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS_HTTP_STREAMER))

ALL_OBJS = $(OBJS_COMMON) $(UDP_PLAYER_OBJS) $(FILE_PLAYER_OBJS) $(HTTP_PLAYER_OBJS) $(OBJS_HTTP_STREAMER)
DEPS = $(patsubst $(OBJDIR)/%.o,$(DEPDIR)/%.d,$(ALL_OBJS))

# ----------------------------------------
# 実行ファイル名
# ----------------------------------------
UDP_PLAYER_BIN    = 7seg-udp-player
FILE_PLAYER_BIN   = 7seg-file-player
HTTP_PLAYER_BIN   = 7seg-http-player
HTTP_STREAMER_BIN = 7seg-http-streamer
TARGETS = $(UDP_PLAYER_BIN) $(FILE_PLAYER_BIN) $(HTTP_PLAYER_BIN) $(HTTP_STREAMER_BIN)


# 追加のターゲット: RTP プレイヤー
RTP_PLAYER_SRCS = $(wildcard $(SRCDIR)/rtp_player.cpp)
RTP_PLAYER_OBJS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(RTP_PLAYER_SRCS))
RTP_PLAYER_BIN  = 7seg-rtp-player
TARGETS += $(RTP_PLAYER_BIN)

$(RTP_PLAYER_OBJS): CXXFLAGS = $(BASE_CXXFLAGS) $(CV_SDL_CFLAGS) $(GST_CFLAGS)

$(RTP_PLAYER_BIN): $(OBJS_COMMON) $(RTP_PLAYER_OBJS)
	@echo "Linking $@..."
	$(CXX) -o $@ $^ $(BASE_LDFLAGS) $(CV_SDL_LIBS) $(GST_LIBS)
	@echo "Successfully built -> $@"
	
# ----------------------------------------
# ターゲットごとのフラグとオブジェクトを定義
# ----------------------------------------
OBJS_CV_SDL = $(OBJS_COMMON) $(UDP_PLAYER_OBJS) $(FILE_PLAYER_OBJS) $(HTTP_PLAYER_OBJS)
$(OBJS_CV_SDL): CXXFLAGS = $(BASE_CXXFLAGS) $(CV_SDL_CFLAGS)

$(OBJS_HTTP_STREAMER): CXXFLAGS = $(BASE_CXXFLAGS) $(GST_CFLAGS)



# ----------------------------------------
# ビルドルール
# ----------------------------------------
.PHONY: all clean package deb

all: $(TARGETS)

# --- 実行ファイルのリンク ---
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
	@echo "Successfully built -> $@"

$(HTTP_STREAMER_BIN): $(OBJS_HTTP_STREAMER)
	@echo "Linking $@..."
	$(CXX) -o $@ $^ $(BASE_LDFLAGS) $(GST_LIBS)
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
	rm -rf $(OBJDIR) $(TARGETS) $(PROJECT_NAME)-*.tar.gz
