# コンパイラとオプション
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
LDFLAGS = -pthread

# 依存ライブラリ (pkg-config を使用)
GST_CFLAGS = $(shell pkg-config --cflags gstreamer-1.0)
GST_LIBS = $(shell pkg-config --libs gstreamer-1.0)

# インクルードパス
INCLUDES = -I./cpp-httplib

# ビルドするソースファイルを明示的に一つだけ指定する
SRCDIR = src
OBJDIR = obj
SOURCES = $(SRCDIR)/http_streamer.cpp
OBJECTS = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SOURCES))

# 実行ファイル名
TARGET = 7seg-http-streamer

# デフォルトターゲット
all: $(TARGET)

# 実行ファイルのリンク
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS) $(GST_LIBS)

# オブジェクトファイルのコンパイル
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(GST_CFLAGS) -c $< -o $@

# クリーンアップ
clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all clean

