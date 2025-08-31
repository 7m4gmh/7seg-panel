# ----------------------------------------
# プロジェクト設定
# ----------------------------------------
PROJECT_NAME = 7seg-panel
VERSION      = 1.0.0

# 利用可能なCPUコア数を自動で取得して、その数で並列ビルドする
# nprocコマンドがない環境のために、失敗したらデフォルトで4にする
NUM_CORES := $(shell nproc 2>/dev/null || echo 4)
MAKEFLAGS += -j$(NUM_CORES)

# ----------------------------------------
# コンパイラと共通設定
# ----------------------------------------
CXX = g++
BASE_CXXFLAGS = -std=c++17 -Wall -O2 -I../cpp-httplib -I./include
BASE_LDFLAGS = -pthread

# ----------------------------------------
# 依存ライブラリごとの設定
# ----------------------------------------
CV_SDL_CFLAGS = $(shell pkg-config --cflags opencv4)
CV_SDL_LIBS   = -lSDL2 $(shell pkg-config --libs opencv4)
GST_CFLAGS = $(shell pkg-config --cflags gstreamer-1.0)
GST_LIBS   = $(shell pkg-config --libs gstreamer-1.0)

# ----------------------------------------
# ターゲットごとの最終的なフラグを定義
# ----------------------------------------
CXXFLAGS_CV_SDL = $(BASE_CXXFLAGS) $(CV_SDL_CFLAGS)
LDFLAGS_CV_SDL  = $(BASE_LDFLAGS) $(CV_SDL_LIBS)
CXXFLAGS_GST = $(BASE_CXXFLAGS) $(GST_CFLAGS)
LDFLAGS_GST  = $(BASE_LDFLAGS) $(GST_LIBS)

# ----------------------------------------
# ソースファイルとオブジェクトファイル
# ----------------------------------------
SRCDIR = src
OBJDIR = obj

SRCS_COMMON = $(SRCDIR)/common.cpp $(SRCDIR)/led.cpp $(SRCDIR)/video.cpp $(SRCDIR)/audio.cpp $(SRCDIR)/playback.cpp
OBJS_COMMON = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS_COMMON))

SERVER_SRCS = $(SRCDIR)/udp_player.cpp $(SRCDIR)/udp.cpp
SERVER_OBJS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SERVER_SRCS))

PLAYER_SRCS = $(SRCDIR)/file_player.cpp
PLAYER_OBJS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(PLAYER_SRCS))

HTTP_PLAYER_SRCS = $(SRCDIR)/http_player.cpp
HTTP_PLAYER_OBJS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(HTTP_PLAYER_SRCS))

HTTP_STREAMER_SRC = $(SRCDIR)/http_streamer.cpp
HTTP_STREAMER_OBJ = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(HTTP_STREAMER_SRC))

# ----------------------------------------
# 実行ファイル名
# ----------------------------------------
SERVER_BIN        = 7seg-udp-player
PLAYER_BIN        = 7seg-file-player
HTTP_PLAYER_BIN   = 7seg-http-player
HTTP_STREAMER_BIN = 7seg-http-streamer
TARGETS = $(SERVER_BIN) $(PLAYER_BIN) $(HTTP_PLAYER_BIN) $(HTTP_STREAMER_BIN)

# ----------------------------------------
# ビルドルール
# ----------------------------------------
.PHONY: all clean package deb

all: $(TARGETS)

# --- 実行ファイルのリンク (★★★ ここが欠落していました ★★★) ---

# (A) OpenCV/SDL2 を使用するターゲット
$(SERVER_BIN): $(OBJS_COMMON) $(SERVER_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS_CV_SDL)
	@echo "Successfully built -> $@"

$(PLAYER_BIN): $(OBJS_COMMON) $(PLAYER_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS_CV_SDL)
	@echo "Successfully built -> $@"

$(HTTP_PLAYER_BIN): $(OBJS_COMMON) $(HTTP_PLAYER_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS_CV_SDL)
	@echo "Successfully built -> $@"

# (B) GStreamer を使用するターゲット
$(HTTP_STREAMER_BIN): $(HTTP_STREAMER_OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS_GST)
	@echo "Successfully built -> $@"

# --- オブジェクトファイルのコンパイル ---

$(OBJDIR):
	@mkdir -p $(OBJDIR)

# (A) OpenCV/SDL2 に依存するソースをコンパイルするルール
$(OBJS_COMMON) $(SERVER_OBJS) $(PLAYER_OBJS) $(HTTP_PLAYER_OBJS): $(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS_CV_SDL) -c $< -o $@

# (B) GStreamer に依存するソースをコンパイルするルール
$(HTTP_STREAMER_OBJ): $(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS_GST) -c $< -o $@

# ----------------------------------------
# パッケージ作成 (ソース)
# ----------------------------------------
PACKAGE       = $(PROJECT_NAME)-$(VERSION)
ARCHIVE       = $(PACKAGE).tar.gz
PACKAGE_FILES = src www Makefile README.md README.en.md README.ja.md README.zh-CN.md docs

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
	@echo "Depends: libopencv-core406, libopencv-videoio406, libsdl2-2.0-0, libgstreamer1.0-0" >> $(DEB_DIR)/DEBIAN/control
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
	rm -rf $(OBJDIR) $(TARGETS) $(PROJECT_NAME)-*.tar.gz $(PROJECT_NAME)_*.deb
