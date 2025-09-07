#include "common.h"
#include "led.h"      // initialize_displays, reset_i2c_channel_cache のために必要
#include <iostream>
#include <unistd.h>   // close, usleep, sleep
#include <fcntl.h>    // open, O_RDWR


std::atomic<bool> finished(false);
std::atomic<bool> audio_waiting(false);
std::mutex audio_mtx;
std::deque<std::vector<char>> audio_buf;

std::mutex frame_mtx;
std::vector<uint8_t> latest_frame;
std::atomic<int> last_pts_ms(0);
double start_time = 0.0;
int audio_bytes_received = 0;

// 共通モジュール I²C アドレス
//const std::vector<int> MODULE_ADDRESSES = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75};

std::atomic<bool> g_should_exit(false);

double CHAR_WIDTH_MM = 12.7;
double CHAR_HEIGHT_MM = 19.2;

// シグナルハンドラ関数
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_should_exit = true;
    }
}

// シグナルハンドラを設定する関数
void setup_signal_handlers() {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

bool attempt_i2c_recovery(int& i2c_fd, const DisplayConfig& config) {
    std::cerr << "I2C communication failed. Starting recovery procedure..." << std::endl;
    
    const int MAX_RECOVERY_RETRIES = 3; // 最大3回までリトライ

    for (int retry_count = 1; retry_count <= MAX_RECOVERY_RETRIES; ++retry_count) {
        std::cout << "[Recovery] Attempt " << retry_count << "/" << MAX_RECOVERY_RETRIES << "..." << std::endl;

        // 1. I2Cファイルディスクリプタを閉じる
        if (i2c_fd >= 0) {
            close(i2c_fd);
        }
        
        // 2. 状態キャッシュをリセット
        reset_i2c_channel_cache();
        
        // 3. (オプション) I2Cバスをリフレッシュ
        // system("i2cdetect -y 0 > /dev/null 2>&1");
        
        // 4. リトライ間隔を設ける
        usleep(retry_count * 500000); // 0.5秒, 1.0秒, 1.5秒...

        // 5. I2Cデバイスを再度オープン
        i2c_fd = open("/dev/i2c-0", O_RDWR);
        if (i2c_fd < 0) {
            perror("[Recovery] Failed to re-open /dev/i2c-0. Retrying...");
            continue; // 次のリトライへ
        }

        // 6. 再初期化を試みる
        if (initialize_displays(i2c_fd, config)) {
            std::cout << "[Recovery] Recovery successful!" << std::endl;
            return true; // ★成功したので true を返して終了
        } else {
            std::cerr << "[Recovery] Re-initialization failed." << std::endl;
        }
    }

    // すべてのリトライが失敗した場合
    std::cerr << "[Recovery] All recovery attempts failed." << std::endl;
    return false; // ★失敗したので false を返して終了
}
