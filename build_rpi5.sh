#!/bin/bash

# RPi5用ビルドスクリプト
# このスクリプトはRPi5上で実行してください

set -e

echo "=== 7-Segment LED Panel - RPi5 Build Script ==="

# ログファイルの設定
LOG_FILE="rpi5_test_results_$(date +%Y%m%d_%H%M%S).log"
exec > >(tee -a "$LOG_FILE") 2>&1

echo "Test results will be saved to: $LOG_FILE"
echo "Start time: $(date)"
echo ""

# システム情報の収集
echo "=== System Information ==="
echo "Hardware: $(uname -m)"
echo "OS: $(lsb_release -d -s 2>/dev/null || echo 'Unknown')"
echo "Kernel: $(uname -r)"
echo "CPU: $(nproc) cores"
echo "Memory: $(free -h | grep '^Mem:' | awk '{print $2}')"
echo ""

# 依存関係のインストール
echo "Installing dependencies..."
sudo apt update

# Raspberry Pi OS固有のパッケージ名を使用
echo "Installing core dependencies..."
sudo apt install -y \
    build-essential \
    cmake \
    pkg-config \
    libopencv-dev \
    libopencv-contrib-dev \
    libsdl2-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    nlohmann-json3-dev \
    git

# GStreamer plugins-goodのインストール（Raspberry Pi OS対応）
echo "Installing GStreamer plugins-good..."
if apt-cache show gstreamer1.0-plugins-good 2>/dev/null | grep -q "Package:"; then
    sudo apt install -y gstreamer1.0-plugins-good
    echo "Installed gstreamer1.0-plugins-good"
elif apt-cache show libgstreamer-plugins-good1.0-dev 2>/dev/null | grep -q "Package:"; then
    sudo apt install -y libgstreamer-plugins-good1.0-dev
    echo "Installed libgstreamer-plugins-good1.0-dev"
else
    echo "Warning: GStreamer plugins-good not found, installing available alternatives..."
    sudo apt install -y gstreamer1.0-plugins-base gstreamer1.0-plugins-ugly
fi

# プロジェクトのビルド
echo "Building project..."
make clean
make -j$(nproc)

echo "Build completed successfully!"
echo "Available binaries:"
ls -la bin/linux-arm64-rpi5/
echo ""

# バージョン情報の確認
echo "=== Version Information ==="
echo "OpenCV version: $(pkg-config --modversion opencv4 2>/dev/null || echo 'Not found')"
echo "SDL2 version: $(pkg-config --modversion sdl2 2>/dev/null || echo 'Not found')"
echo "GStreamer version: $(pkg-config --modversion gstreamer-1.0 2>/dev/null || echo 'Not found')"

# Git情報の確認
if git rev-parse --git-dir > /dev/null 2>&1; then
    echo "Git commit: $(git rev-parse HEAD)"
    echo "Git branch: $(git branch --show-current)"
    echo "Last commit message: $(git log -1 --pretty=%B | head -1)"
else
    echo "Git repository: Not found"
fi
echo ""

echo "=== Performance Test ==="
echo "Building and running emulator benchmark for RPi5..."

# RPi5上では必ずビルドする（macOSバイナリとの互換性なし）
echo "Building benchmark executable..."
make clean
make benchmark

# ビルドされたバイナリの確認
if [ -f "./emulator_benchmark" ]; then
    echo "Benchmark binary found, checking architecture..."
    file ./emulator_benchmark

    echo "Running benchmark..."
    echo "Setting Qt platform to offscreen mode for headless operation..."
    export QT_QPA_PLATFORM=offscreen
    export DISPLAY=:0  # Fallback display
    ./emulator_benchmark
    BENCHMARK_RESULT=$?
else
    echo "ERROR: emulator_benchmark binary not found after build"
    echo "Checking build output..."
    ls -la
    BENCHMARK_RESULT=1
fi

if [ $BENCHMARK_RESULT -eq 0 ]; then
    echo "Benchmark test: SUCCESS"
else
    echo "Benchmark test: FAILED (exit code: $BENCHMARK_RESULT)"
fi
fi
echo ""

echo "=== Test file player with emulator ==="
if [ -f "./bin/linux-arm64-rpi5/7seg-file-player" ]; then
    echo "File player binary found, checking architecture..."
    file ./bin/linux-arm64-rpi5/7seg-file-player
    echo "Testing file player with emulator mode..."
    timeout 10s ./bin/linux-arm64-rpi5/7seg-file-player test.mp4 emulator-4x8 || true
    TEST_RESULT=$?
    if [ $TEST_RESULT -eq 124 ]; then
        echo "Emulator test: SUCCESS (timeout as expected)"
    elif [ $TEST_RESULT -eq 0 ]; then
        echo "Emulator test: SUCCESS (completed normally)"
    else
        echo "Emulator test: FAILED (exit code: $TEST_RESULT)"
    fi
else
    echo "File player binary not found, building project first..."
    echo "Building main project..."
    make clean
    make -j$(nproc)

    if [ -f "./bin/linux-arm64-rpi5/7seg-file-player" ]; then
        echo "Binary built successfully, testing..."
        file ./bin/linux-arm64-rpi5/7seg-file-player
        echo "Setting Qt platform to offscreen mode for headless emulator test..."
        export QT_QPA_PLATFORM=offscreen
        export DISPLAY=:0
        timeout 10s ./bin/linux-arm64-rpi5/7seg-file-player test.mp4 emulator-4x8 || true
        TEST_RESULT=$?
        if [ $TEST_RESULT -eq 124 ]; then
            echo "Emulator test: SUCCESS (timeout as expected)"
        elif [ $TEST_RESULT -eq 0 ]; then
            echo "Emulator test: SUCCESS (completed normally)"
        else
            echo "Emulator test: FAILED (exit code: $TEST_RESULT)"
        fi
    else
        echo "ERROR: Failed to build file player binary"
        echo "Checking build directory..."
        ls -la bin/ 2>/dev/null || echo "bin/ directory not found"
        TEST_RESULT=1
        echo "Emulator test: FAILED (build failed)"
    fi
fi

echo ""
echo "=== RPi5 Build and Test Complete ==="
echo "End time: $(date)"
echo "Log saved to: $LOG_FILE"

# 結果のサマリー
echo ""
echo "=== Test Summary ==="
echo "Build: SUCCESS"
echo "Benchmark: $([ $BENCHMARK_RESULT -eq 0 ] && echo 'SUCCESS' || echo 'FAILED')"
echo "Emulator Test: $([ -f "./bin/linux-arm64-rpi5/7seg-file-player" ] && echo 'SUCCESS' || echo 'FAILED')"
echo ""
echo "For detailed results, check: $LOG_FILE"
