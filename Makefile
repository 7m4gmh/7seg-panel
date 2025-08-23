# ----------------------------------------
# コンパイラと共通設定
# ----------------------------------------
CXX = g++
# 共通のベースとなるコンパイルフラグ
BASE_CXXFLAGS = -std=c++17 -Wall -O2 -I../cpp-httplib
# 共通のベースとなるリンカフラグ
BASE_LDFLAGS = -pthread

# ----------------------------------------
# 依存ライブラリごとの設定
# ----------------------------------------
# (A) OpenCV と SDL2 を使用するターゲット用
CV_SDL_CFLAGS = $(shell pkg-config --cflags opencv4)
CV_SDL_LIBS   = -lSDL2 $(shell pkg-config --libs opencv4)

# (B) GStreamer を使用するターゲット用
GST_CFLAGS = $(shell pkg-config --cflags gstreamer-1.0)
GST_LIBS   = $(shell pkg-config --libs gstreamer-1.0)

# ----------------------------------------
# ターゲットごとの最終的なフラグを定義
# ----------------------------------------
# (A) OpenCV/SDL2 用のフラグ
CXXFLAGS_CV_SDL = $(BASE_CXXFLAGS) $(CV_SDL_CFLAGS)
LDFLAGS_CV_SDL  = $(BASE_LDFLAGS) $(CV_SDL_LIBS)
# SSLサポートが必要な場合は、以下の2行のコメントを外してください
# LDFLAGS_CV_SDL += -lssl -lcrypto
# CXXFLAGS_CV_SDL += -DCPPHTTPLIB_OPENSSL_SUPPORT

# (B) GStreamer 用のフラグ
CXXFLAGS_GST = $(BASE_CXXFLAGS) $(GST_CFLAGS)
LDFLAGS_GST  = $(BASE_LDFLAGS) $(GST_LIBS)

# ----------------------------------------
# ソースファイルとオブジェクトファイル
# ----------------------------------------
SRCDIR = src
OBJDIR = obj

# (A) OpenCV/SDL2 を使用するターゲットのソース
SRCS_COMMON = $(SRCDIR)/common.cpp $(SRCDIR)/led.cpp $(SRCDIR)/video.cpp $(SRCDIR)/audio.cpp
OBJS_COMMON = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS_COMMON))

SERVER_SRCS = $(SRCDIR)/server.cpp $(SRCDIR)/udp.cpp
SERVER_OBJS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SERVER_SRCS))

PLAYER_SRCS = $(SRCDIR)/player.cpp
PLAYER_OBJS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(PLAYER_SRCS))

HTTP_PLAYER_SRCS = $(SRCDIR)/http_player.cpp
HTTP_PLAYER_OBJS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(HTTP_PLAYER_SRCS))

# (B) GStreamer を使用するターゲットのソース
HTTP_STREAMER_SRC = $(SRCDIR)/http_streamer.cpp
HTTP_STREAMER_OBJ = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(HTTP_STREAMER_SRC))

# ----------------------------------------
# 実行ファイル名
# ----------------------------------------
SERVER_BIN        = 7seg-udp-server
PLAYER_BIN        = 7seg-file-player
HTTP_PLAYER_BIN   = 7seg-http-player
HTTP_STREAMER_BIN = 7seg-http-streamer

TARGETS = $(SERVER_BIN) $(PLAYER_BIN) $(HTTP_PLAYER_BIN) $(HTTP_STREAMER_BIN)

# ----------------------------------------
# ビルドルール
# ----------------------------------------
.PHONY: all clean

all: $(TARGETS)

# --- 実行ファイルのリンク ---

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

# (B) GStreamer を使用するターゲット (正しいMakefileの内容を反映)
$(HTTP_STREAMER_BIN): $(HTTP_STREAMER_OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS_GST)
	@echo "Successfully built -> $@"

# --- オブジェクトファイルのコンパイル ---

# $(OBJDIR) ディレクトリがなければ作成する
$(OBJDIR):
	@mkdir -p $(OBJDIR)

# (A) OpenCV/SDL2 に依存するソースをコンパイルするルール
$(OBJS_COMMON) $(SERVER_OBJS) $(PLAYER_OBJS) $(HTTP_PLAYER_OBJS): $(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS_CV_SDL) -c $< -o $@

# (B) GStreamer に依存するソースをコンパイルするルール
$(HTTP_STREAMER_OBJ): $(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS_GST) -c $< -o $@

# ----------------------------------------
# クリーンアップ
# ----------------------------------------
clean:
	rm -rf $(OBJDIR) $(TARGETS)

