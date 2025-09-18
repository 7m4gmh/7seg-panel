#include "emulator_display.h"
#include <vector>
#include <chrono>
#include <iostream>
#include <random>

// パフォーマンスベンチマーク関数
void run_performance_benchmark() {
    const int ROWS = 4;
    const int COLS = 8;
    const int TOTAL_DIGITS = ROWS * COLS;
    const int FRAMES = 1000; // ベンチマーク用のフレーム数

    // エミュレータ表示クラスを作成
    auto display = create_emulator_display(ROWS, COLS);

    // ランダムデータ生成器
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    // ベンチマーク開始
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int frame = 0; frame < FRAMES; ++frame) {
        // ランダムなグリッドデータを生成
        std::vector<uint8_t> grid(TOTAL_DIGITS);
        for (int i = 0; i < TOTAL_DIGITS; ++i) {
            grid[i] = dis(gen);
        }

        // 表示更新
        display->update(grid);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // 結果出力
    double fps = static_cast<double>(FRAMES) / (duration.count() / 1000.0);
    double avg_frame_time = static_cast<double>(duration.count()) / FRAMES;

    std::cout << "=== Performance Benchmark Results ===" << std::endl;
    std::cout << "Frames: " << FRAMES << std::endl;
    std::cout << "Total time: " << duration.count() << " ms" << std::endl;
    std::cout << "Average FPS: " << fps << std::endl;
    std::cout << "Average frame time: " << avg_frame_time << " ms" << std::endl;
    std::cout << "====================================" << std::endl;

    delete display;
}

int main() {
    std::cout << "Starting emulator performance benchmark..." << std::endl;
    run_performance_benchmark();
    return 0;
}
