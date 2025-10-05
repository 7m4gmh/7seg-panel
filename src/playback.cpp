// src/playback.cpp
#include "playback.h"
#include "common.h"
#include "led.h"
#include "video.h" // ★★★ 'frame_to_grid' のために必要 ★★★
#include "file_audio_gst.h"
#include "audio.h"
#include <opencv2/opencv.hpp> // ★★★ 'cv::' 関連 ★★★
#include <opencv2/core/ocl.hpp> // ★★★ GPU/OpenCLサポート用 ★★★
#include <iostream>
#include <chrono>             // ★★★ 'std::chrono' のために必要 ★★★
#include <thread>             // ★★★ 'std::this_thread' のために必要 ★★★
#include <map>    // std::map のために必要
#include <utility> // std::pair のために必要
#include <csignal> // signal のために必要
#include <unistd.h>
#include <fcntl.h>
#include <atomic>             // std::atomic のために念のため

// Python版と同じ比率・スケール・オフセットでセグメント座標を計算
// グローバル定数
constexpr int CANVAS_W = 200;
constexpr int CANVAS_H = 300;

constexpr double UNIT_W = 12.7; // 1桁あたりの横幅（物理パッケージ幅）
constexpr double UNIT_H = 19.05; // 1桁あたりの縦幅
constexpr double SEG_L = 6.0;   // 横セグメント長さ
constexpr double SEG_W = 2;   // セグメント幅
constexpr double TILT = 10.0;   // degree
constexpr double X_RANGE = 3.0;
//constexpr double Y_RANGE = 4.0; // unused, removed to avoid warnings
constexpr double SCALE = 8.0; // スケール値を調整

inline int seg_x(double x) {
    int result = CANVAS_W / 2 + static_cast<int>(x * SCALE);
    return std::clamp(result, 0, CANVAS_W); // キャンバス範囲内に制限
}

inline int seg_y(double y) {
    int result = CANVAS_H / 2 - static_cast<int>(y * SCALE);
    return std::clamp(result, 0, CANVAS_H); // キャンバス範囲内に制限
}

std::vector<cv::Point> horizontal_segment(double x, double y) {
    // 六角形（端2点が90度、他4点が135度）
    double w = SEG_L * SCALE;
    double h = SEG_W * SCALE;
    double cut = h / 2.0;
    std::vector<cv::Point> pts = {
        {int(x - w/2 + cut + 0.5), int(y - h/2 + 0.5)},
        {int(x + w/2 - cut + 0.5), int(y - h/2 + 0.5)},
        {int(x + w/2 + 0.5),      int(y + 0.5)},
        {int(x + w/2 - cut + 0.5), int(y + h/2 + 0.5)},
        {int(x - w/2 + cut + 0.5), int(y + h/2 + 0.5)},
        {int(x - w/2 + 0.5),      int(y + 0.5)}
    };
    return pts;
}

std::vector<cv::Point> vertical_segment(double x, double y, double tilt_deg) {
    // 横セグメント六角形を重心中心で90±tilt度回転
    auto base = horizontal_segment(0, 0);
    // 重心計算
    double cx = 0, cy = 0;
    for(const auto& pt : base) { cx += pt.x; cy += pt.y; }
    cx /= base.size();
    cy /= base.size();
    double angle = (90.0 + tilt_deg) * CV_PI / 180.0;
    std::vector<cv::Point> pts;
    for(const auto& pt : base) {
        double px = pt.x - cx, py = pt.y - cy;
        double qx = std::cos(angle) * px - std::sin(angle) * py;
        double qy = std::sin(angle) * px + std::cos(angle) * py;
        pts.emplace_back(int(x + qx + 0.5), int(y + qy + 0.5));
    }
    return pts;
}

struct SegmentLayout {
    std::vector<std::vector<cv::Point>> segs; // a-g
    cv::Point dp_center;
    int dp_radius;
};

// 全桁分のセグメントレイアウトをキャッシュするための構造体
struct CachedDisplayLayout {
    std::vector<std::vector<std::vector<cv::Point>>> all_segs; // [digit_idx][segment_idx][points]
    std::vector<cv::Point> all_dp_centers;
    std::vector<int> all_dp_radii;
    std::vector<std::pair<double, double>> package_centers;
    int window_width;
    int window_height;
};

SegmentLayout make_layout(int digit_idx, double package_center_x, double package_center_y) {
    // --- まず座標・補正量をすべて宣言 ---
    // tilt calculations not required in this implementation (previously unused)
    double digit_spacing = UNIT_W * SCALE;
    double dx = digit_idx * digit_spacing;
    double margin = 0.0; // 任意のマージン（mm単位）
    double y_top =  (UNIT_H/2.0 - SEG_L/2.0 + SEG_W/2 - margin);
    double y_bot = -(UNIT_H/2.0 - SEG_L/2.0 + SEG_W/2 - margin);
    double x_rgt =  (UNIT_W/2.0 - SEG_L/2.0 + SEG_W/2 - margin);
    double x_lft = -(UNIT_W/2.0 - SEG_L/2.0 + SEG_W/2 - margin);

    // --- セグメント形状を一度だけ定義 ---
    auto seg_b_shape = vertical_segment(0, 0, TILT);
    auto seg_f_shape = vertical_segment(seg_x(x_lft+0.5) + dx, seg_y(y_top-2 - SEG_W/1), TILT);
    auto seg_c_shape = vertical_segment(seg_x(x_rgt-0.5) + dx, seg_y(y_bot+2 + SEG_W/1), TILT);
    auto seg_e_shape = vertical_segment(seg_x(x_lft+0.5) + dx, seg_y(y_bot+2 + SEG_W/1), TILT);

    // b_dx計算
    int b_top_idx = 0, b_bot_idx = 0;
    for (int i = 1; i < seg_b_shape.size(); ++i) {
        if (seg_b_shape[i].y < seg_b_shape[b_top_idx].y) b_top_idx = i;
        if (seg_b_shape[i].y > seg_b_shape[b_bot_idx].y) b_bot_idx = i;
    }
    int b_dx = seg_b_shape[b_bot_idx].x - seg_b_shape[b_top_idx].x;

    // A: 上横（中央寄せ, 左へb_dx平行移動）
    auto seg_a_base = horizontal_segment(seg_x(0.5) + dx, seg_y(y_top - SEG_W/2));
    std::vector<cv::Point> seg_a;
    for (const auto& pt : seg_a_base) seg_a.emplace_back(pt.x - b_dx, pt.y);

    // セグメント座標を一度だけ定義
    // B: 右上縦（+TILT, 中央寄せ, 右へb_dx平行移動）
    auto seg_b_base = vertical_segment(seg_x(x_rgt-0.5) + dx, seg_y(y_top-2 - SEG_W/1), TILT);
    std::vector<cv::Point> seg_b;
    for (const auto& pt : seg_b_base) seg_b.emplace_back(pt.x - b_dx, pt.y);
    // F: 左上縦（+TILT, 右へb_dx平行移動）
    std::vector<cv::Point> seg_f;
    for (const auto& pt : seg_f_shape) seg_f.emplace_back(pt.x - b_dx, pt.y);
    // C: 右下縦（+TILT, 左へb_dx平行移動）
    std::vector<cv::Point> seg_c;
    for (const auto& pt : seg_c_shape) seg_c.emplace_back(pt.x + b_dx, pt.y);
    // E: 左下縦（+TILT, 左へb_dx平行移動）
    std::vector<cv::Point> seg_e;
    for (const auto& pt : seg_e_shape) seg_e.emplace_back(pt.x + b_dx, pt.y);

    // D: 下横（中央寄せ, 右へb_dx平行移動）
    auto seg_d_base = horizontal_segment(seg_x(-0.5) + dx, seg_y(y_bot + SEG_W/2));
    std::vector<cv::Point> seg_d;
    for (const auto& pt : seg_d_base) seg_d.emplace_back(pt.x + b_dx, pt.y);

    auto seg_g = horizontal_segment(seg_x(0) + dx, seg_y(0));

    std::vector<std::vector<cv::Point>> segs = {
        seg_a, // a: 上横
        seg_b, // b: 右上縦
        seg_c, // c: 右下縦
        seg_d, // d: 下横
        seg_e, // e: 左下縦
        seg_f, // f: 左上縦
        seg_g, // g: 中央横
    };
    // dp: dセグメントと同じ高さ、右端から左に1単位ずらす
    int d_cy = seg_y(y_bot);
    int dp_r = int((SEG_W * SCALE) / 2.0 + 0.5);
    int dp_cx = seg_x(X_RANGE) + dx + int(SCALE * 1.0 + 0.5);
    cv::Point dp_center(dp_cx, d_cy);

    // segment G の重心を計算
    const auto& g = segs[6];
    double gx = 0, gy = 0;
    for(const auto& pt : g) { gx += pt.x; gy += pt.y; }
    gx /= g.size();
    gy /= g.size();
    cv::Point g_center(gx, gy);

    // 平行移動量
    int dx_shift = static_cast<int>(package_center_x - gx + 0.5);
    int dy_shift = static_cast<int>(package_center_y - gy + 0.5);
    // 全セグメントと小数点を平行移動
    for(auto& seg : segs) for(auto& pt : seg) { pt.x += dx_shift; pt.y += dy_shift; }
    dp_center.x += dx_shift; dp_center.y += dy_shift;

    return {segs, dp_center, dp_r};
}

// 全桁分のセグメントレイアウトを事前計算してキャッシュする関数
CachedDisplayLayout create_cached_layout(const DisplayConfig& config) {
    CachedDisplayLayout cache;
    
    // ウィンドウサイズを計算
    cache.window_width = static_cast<int>(config.total_width * UNIT_W * SCALE);
    cache.window_height = static_cast<int>(config.total_height * UNIT_H * SCALE);
    
    // パッケージ中心座標を計算
    double digit_spacing_x = UNIT_W * SCALE;
    double digit_spacing_y = UNIT_H * SCALE;
    for (int r = 0; r < config.total_height; ++r) {
        for (int c = 0; c < config.total_width; ++c) {
            double package_center_x = c * digit_spacing_x + (digit_spacing_x / 2.0);
            double package_center_y = r * digit_spacing_y + (digit_spacing_y / 2.0);
            cache.package_centers.emplace_back(package_center_x, package_center_y);
        }
    }
    
    // 全桁分のセグメントレイアウトを計算
    int total_digits = config.total_width * config.total_height;
    cache.all_segs.resize(total_digits);
    cache.all_dp_centers.resize(total_digits);
    cache.all_dp_radii.resize(total_digits);
    
    for (int idx = 0; idx < total_digits; ++idx) {
        auto [package_center_x, package_center_y] = cache.package_centers[idx];
        auto layout = make_layout(0, package_center_x, package_center_y);
        cache.all_segs[idx] = layout.segs;
        cache.all_dp_centers[idx] = layout.dp_center;
        cache.all_dp_radii[idx] = layout.dp_radius;
    }
    
    return cache;
}

std::atomic<bool>* g_current_stop_flag = nullptr;
//std::map<std::pair<int, int>, int> g_error_counts;

/*
void handle_signal(int signal) {
    if (signal == SIGINT) {
        // グローバルポインタが設定されていれば、
        // それが指す先のstop_flagをtrueにする
        if (g_current_stop_flag) {
            *g_current_stop_flag = true;
        }
    }
}
*/

// テキストベースの7セグメントエミュレータ表示関数
void display_text_emulator(const std::vector<uint8_t>& grid, const DisplayConfig& config) {
    // この関数は削除されました
}

// 共通の動画再生ロジック
int play_video_stream(const std::string& video_path, const DisplayConfig& config, std::atomic<bool>& stop_flag, 
                     ScalingMode scaling_mode, int min_threshold, int max_threshold, bool debug) {
    g_current_stop_flag = &stop_flag;
    stop_flag = false;

    int i2c_fd = open_i2c_auto(config);
    if (i2c_fd < 0) {
        std::cerr << "I2C communication failed: No I2C devices found or access denied." << std::endl;
        std::cerr << "Please check:" << std::endl;
        std::cerr << "  - I2C hardware is connected and powered" << std::endl;
        std::cerr << "  - You have permission to access I2C devices" << std::endl;
        std::cerr << "  - Use emulator mode if you don't have hardware: ./7seg-http-player <path> emulator-<width>x<height>" << std::endl;
        return -1;
    }

    if (!initialize_displays(i2c_fd, config)) {
        std::cerr << "Failed to initialize display modules." << std::endl;
        close(i2c_fd);
        return -1;
    }

    cv::VideoCapture cap;
    if (video_path == "-") {
        std::cout << "標準入力からビデオストリームを読み込みます..." << std::endl;
        // GStreamerパイプラインを使って標準入力(fd=0)から読み込む
        const std::string gst_pipeline = 
             "fdsrc ! decodebin name=d "
            "d. ! queue ! videoconvert ! appsink "
            "d. ! queue ! audioconvert ! audioresample ! autoaudiosink";
        cap.open(gst_pipeline, cv::CAP_GSTREAMER);
    } else {
        cap.open(video_path, cv::CAP_FFMPEG);
    }

    if (!cap.isOpened()) {
        std::cerr << "動画ソースを開けません: " << video_path << std::endl;
        close(i2c_fd);
        return -1;
    }

    // ローカルファイル時の音声: 既定では SDL 再生（環境変数でFFPLAYへ切替）。SDL開始失敗時はffplayへフォールバック。
    bool use_ffplay = false;
    if (const char* e = std::getenv("FILE_AUDIO_USE_FFPLAY")) {
        if (std::string(e) == "1" || std::string(e) == "true") use_ffplay = true;
    }
    if (video_path != "-") {
        if (use_ffplay) {
            std::string command = "ffplay -nodisp -autoexit \"" + video_path + "\" > /dev/null 2>&1 &";
            system(command.c_str());
        } else {
            // SDL(Audio) 経由再生（失敗時はffplayへ）
            bool inited = audio_init(SAMPLE_RATE, CHANNELS);
            bool ok = inited && file_audio_start(video_path);
            if (!ok) {
                if (inited) audio_cleanup();
                std::string command = "ffplay -nodisp -autoexit \"" + video_path + "\" > /dev/null 2>&1 &";
                system(command.c_str());
                use_ffplay = true;
            }
        }
    }

    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0) fps = 30.0;
    auto frame_duration = std::chrono::microseconds(static_cast<long long>(1000000.0 / fps));
    std::cout << "再生開始: " << video_path << " (" << fps << " FPS)" << std::endl;

    auto next_frame_time = std::chrono::steady_clock::now();
    cv::Mat frame;

    // g_should_exit (Ctrl+C) と stop_flag (ロジックによる停止) の両方をチェック
    while (!g_should_exit && !stop_flag && cap.read(frame)) {
        cv::Mat cropped_frame;
        float source_aspect = static_cast<float>(frame.cols) / frame.rows;
        const float target_aspect = static_cast<float>(config.total_width * CHAR_WIDTH_MM) / (config.total_height * CHAR_HEIGHT_MM);

            if (scaling_mode == ScalingMode::CROP) {
            // アスペクト比を維持してトリミング
            if (source_aspect > target_aspect) {
                int new_width = static_cast<int>(frame.rows * target_aspect);
                int x = (frame.cols - new_width) / 2;
                cv::Rect crop_region(x, 0, new_width, frame.rows);
                cropped_frame = frame(crop_region);
            } else {
                int new_height = static_cast<int>(frame.cols / target_aspect);
                int y = (frame.rows - new_height) / 2;
                cv::Rect crop_region(0, y, frame.cols, new_height);
                cropped_frame = frame(crop_region);
            }
        } else if (scaling_mode == ScalingMode::STRETCH) {
            // アスペクト比を無視してディスプレイ領域いっぱいに表示する
            // display の物理アスペクト比に合わせた ROI サイズを計算し、
            // そのサイズに対して動画を非保持アスペクトでリサイズ（ストレッチ）して渡す。
            const double display_aspect = (config.total_width * CHAR_WIDTH_MM) / (config.total_height * CHAR_HEIGHT_MM);
            int roi_w, roi_h, roi_x, roi_y;
            if (static_cast<double>(W) / static_cast<double>(H) > display_aspect) {
                roi_h = H;
                roi_w = std::max(1, static_cast<int>(std::round(roi_h * display_aspect)));
                roi_x = (W - roi_w) / 2;
                roi_y = 0;
            } else {
                roi_w = W;
                roi_h = std::max(1, static_cast<int>(std::round(roi_w / display_aspect)));
                roi_x = 0;
                roi_y = (H - roi_h) / 2;
            }
            if (debug) std::cerr << "[STRETCH] source_aspect=" << source_aspect << " display_aspect=" << display_aspect
                      << " roi=("<<roi_x<<","<<roi_y<<","<<roi_w<<","<<roi_h<<")\n";
            cv::Mat stretched;
            // 非アスペクト保持でリサイズ -> 結果は display の領域サイズ (roi_w x roi_h)
            cv::resize(frame, stretched, cv::Size(roi_w, roi_h));
            cropped_frame = stretched;
            } else { // FIT
            // アスペクト比を維持して全体を表示（余白可能）
            // display の物理アスペクト比に合わせた ROI を W x H キャンバス上に作り、
            // その ROI 内に動画をアスペクト比を保って FIT して中央に貼り付ける。
            // 最後に ROI 部分だけを抜き出して次段に渡す（frame_to_grid が正しい割合で動作するようにする）。
            const double display_aspect = (config.total_width * CHAR_WIDTH_MM) / (config.total_height * CHAR_HEIGHT_MM);

            // W x H のキャンバス
            cv::Mat fit_canvas = cv::Mat::zeros(H, W, frame.type());

            // キャンバス上の ROI を決定
            int roi_w, roi_h, roi_x, roi_y;
            if (static_cast<double>(W) / static_cast<double>(H) > display_aspect) {
                // キャンバスは表示より横長 -> ROI の高さを H に合わせる
                roi_h = H;
                roi_w = std::max(1, static_cast<int>(std::round(roi_h * display_aspect)));
                roi_x = (W - roi_w) / 2;
                roi_y = 0;
            } else {
                // キャンバスは表示より縦長 -> ROI の幅を W に合わせる
                roi_w = W;
                roi_h = std::max(1, static_cast<int>(std::round(roi_w / display_aspect)));
                roi_x = 0;
                roi_y = (H - roi_h) / 2;
            }

            // ROI 内に動画を FIT（縮小）して中央寄せ
            double scale2 = std::min(static_cast<double>(roi_w) / frame.cols, static_cast<double>(roi_h) / frame.rows);
            int dst_w = std::max(1, static_cast<int>(frame.cols * scale2 + 0.5));
            int dst_h = std::max(1, static_cast<int>(frame.rows * scale2 + 0.5));
            if (debug) std::cerr << "[FIT] source_aspect=" << source_aspect << " display_aspect=" << display_aspect
                      << " roi=("<<roi_x<<","<<roi_y<<","<<roi_w<<","<<roi_h<<") dst=("<<dst_w<<","<<dst_h<<")\n";

            cv::Mat scaled_frame;
            cv::resize(frame, scaled_frame, cv::Size(dst_w, dst_h));
            int paste_x = roi_x + (roi_w - dst_w) / 2;
            int paste_y = roi_y + (roi_h - dst_h) / 2;
            scaled_frame.copyTo(fit_canvas(cv::Rect(paste_x, paste_y, dst_w, dst_h)));

            // 重要: ROI 部分だけを抜き出して渡す。これで frame_to_grid が期待通りの領域を扱う。
            cropped_frame = fit_canvas(cv::Rect(roi_x, roi_y, roi_w, roi_h)).clone();
        }

        cv::Mat resized_frame, gray_frame, bw_frame;
        // FIT/CROP は既にアスペクトを display に合わせた ROI を渡しているため、
        // ここで無理に W x H にリサイズするとアスペクト比が崩れて潰れた表示になる。
        // そのため FIT と CROP の場合はリサイズをスキップして、切り出した画像をそのまま渡す。
        if (scaling_mode == ScalingMode::FIT || scaling_mode == ScalingMode::CROP || scaling_mode == ScalingMode::STRETCH) {
            resized_frame = cropped_frame;
        } else {
            cv::resize(cropped_frame, resized_frame, cv::Size(W, H));
        }
        cv::cvtColor(resized_frame, gray_frame, cv::COLOR_BGR2GRAY);
        cv::threshold(gray_frame, bw_frame, min_threshold, max_threshold, cv::THRESH_BINARY);

        std::vector<uint8_t> grid;
        frame_to_grid(bw_frame, config, grid);
   
        I2CErrorInfo error_info;

        if (!update_flexible_display(i2c_fd, config, grid, error_info)) {
               if (error_info.error_occurred) {
                //g_error_counts[{error_info.channel, error_info.address}]++;
            }
            if (!attempt_i2c_recovery(i2c_fd, config)) {
            // 全ての復旧に失敗した場合、長めに待つ
            std::cerr << "Recovery failed. Pausing before next attempt..." << std::endl;
            sleep(2);    
            }   
        }
        next_frame_time += frame_duration;
        std::this_thread::sleep_until(next_frame_time);
    }

    if (video_path != "-") {
        if (use_ffplay) {
            system("killall ffplay > /dev/null 2>&1");
        } else {
            file_audio_stop();
            audio_cleanup();
        }
    }
    cap.release();
    close(i2c_fd);

    if (stop_flag) {
        std::cout << "再生中止: " << video_path << std::endl;
    } else {
        std::cout << "再生終了: " << video_path << std::endl;
    }
    //handle_signal(SIGINT);
    g_current_stop_flag = nullptr;
    return 0;
}

int play_video_stream_emulator(const std::string& video_path, const DisplayConfig& config, std::atomic<bool>& stop_flag,
                              ScalingMode scaling_mode, int min_threshold, int max_threshold, bool debug) {
    g_current_stop_flag = &stop_flag;
    stop_flag = false;

    // GPU/OpenCLを有効化
    cv::ocl::setUseOpenCL(true);
    std::cout << "OpenCV GPU acceleration: " 
              << (cv::ocl::haveOpenCL() ? "ENABLED" : "DISABLED") << std::endl;
    
    if (cv::ocl::haveOpenCL()) {
        cv::ocl::Context context = cv::ocl::Context::getDefault();
        if (!context.empty()) {
            cv::ocl::Device device = context.device(0);
            std::cout << "GPU Device: " << device.name() 
                      << " (" << (device.type() == cv::ocl::Device::TYPE_GPU ? "GPU" : "CPU") << ")" 
                      << std::endl;
        }
    }

    // 起動時に全桁分のセグメントレイアウトを計算してキャッシュ
    CachedDisplayLayout cache = create_cached_layout(config);

    cv::VideoCapture cap;
    if (video_path == "-") {
        std::cout << "標準入力からビデオストリームを読み込みます..." << std::endl;
        const std::string gst_pipeline = 
             "fdsrc ! decodebin name=d "
            "d. ! queue ! videoconvert ! appsink "
            "d. ! queue ! audioconvert ! audioresample ! autoaudiosink";
        cap.open(gst_pipeline, cv::CAP_GSTREAMER);
    } else {
        cap.open(video_path, cv::CAP_FFMPEG);
    }

    if (!cap.isOpened()) {
        std::cerr << "動画ソースを開けません: " << video_path << std::endl;
        return -1;
    }

    // ローカルファイル時の音声: 既定では SDL 再生（環境変数でFFPLAYへ切替）。SDL開始失敗時はffplayへフォールバック。
    bool use_ffplay = false;
    if (const char* e = std::getenv("FILE_AUDIO_USE_FFPLAY")) {
        if (std::string(e) == "1" || std::string(e) == "true") use_ffplay = true;
    }
    if (video_path != "-") {
        if (use_ffplay) {
            std::string command = "ffplay -nodisp -autoexit \"" + video_path + "\" > /dev/null 2>&1 &";
            system(command.c_str());
        } else {
            // SDL(Audio) 経由再生（失敗時はffplayへ）
            bool inited = audio_init(SAMPLE_RATE, CHANNELS);
            bool ok = inited && file_audio_start(video_path);
            if (!ok) {
                if (inited) audio_cleanup();
                std::string command = "ffplay -nodisp -autoexit \"" + video_path + "\" > /dev/null 2>&1 &";
                system(command.c_str());
                use_ffplay = true;
            }
        }
    }

    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0) fps = 30.0;
    auto frame_duration = std::chrono::microseconds(static_cast<long long>(1000000.0 / fps));
    std::cout << "エミュレータ再生開始: " << video_path << " (" << fps << " FPS)" << std::endl;

    auto next_frame_time = std::chrono::steady_clock::now();
    cv::Mat frame;

    // 音声同期のための変数
    auto start_time = std::chrono::steady_clock::now();
    long long frame_count = 0;

    // g_should_exit (Ctrl+C) と stop_flag (ロジックによる停止) の両方をチェック
    while (!g_should_exit && !stop_flag && cap.read(frame)) {
        // 音声同期: 現在の再生時間を計算
        auto current_time_sync = std::chrono::steady_clock::now();
        auto elapsed = current_time_sync - start_time;
        double expected_frame_time = frame_count / fps;
        double actual_elapsed = std::chrono::duration<double>(elapsed).count();
        
        // 動画が音声より遅れている場合、フレームをスキップして追いつく
        if (actual_elapsed > expected_frame_time + 0.1) { // 100ms以上の遅れ
            frame_count++;
            continue; // 次のフレームを処理
        }
        
        cv::Mat cropped_frame;
        float source_aspect = static_cast<float>(frame.cols) / frame.rows;
        const float target_aspect = static_cast<float>(config.total_width * CHAR_WIDTH_MM) / (config.total_height * CHAR_HEIGHT_MM);

            if (scaling_mode == ScalingMode::CROP) {
            // アスペクト比を維持してトリミング
            if (source_aspect > target_aspect) {
                int new_width = static_cast<int>(frame.rows * target_aspect);
                int x = (frame.cols - new_width) / 2;
                cv::Rect crop_region(x, 0, new_width, frame.rows);
                cropped_frame = frame(crop_region);
            } else {
                int new_height = static_cast<int>(frame.cols / target_aspect);
                int y = (frame.rows - new_height) / 2;
                cv::Rect crop_region(0, y, frame.cols, new_height);
                cropped_frame = frame(crop_region);
            }
        } else if (scaling_mode == ScalingMode::STRETCH) {
            // エミュレータ側でもストレッチは display の物理アスペクトに合わせて
            const double display_aspect = (config.total_width * CHAR_WIDTH_MM) / (config.total_height * CHAR_HEIGHT_MM);
            int roi_w, roi_h, roi_x, roi_y;
            if (static_cast<double>(W) / static_cast<double>(H) > display_aspect) {
                roi_h = H;
                roi_w = std::max(1, static_cast<int>(std::round(roi_h * display_aspect)));
                roi_x = (W - roi_w) / 2;
                roi_y = 0;
            } else {
                roi_w = W;
                roi_h = std::max(1, static_cast<int>(std::round(roi_w / display_aspect)));
                roi_x = 0;
                roi_y = (H - roi_h) / 2;
            }
            if (debug) std::cerr << "[STRETCH-emu] source_aspect=" << source_aspect << " display_aspect=" << display_aspect
                      << " roi=("<<roi_x<<","<<roi_y<<","<<roi_w<<","<<roi_h<<")\n";
            cv::Mat stretched;
            cv::resize(frame, stretched, cv::Size(roi_w, roi_h));
            cropped_frame = stretched;
            } else { // FIT
            // アスペクト比を維持して全体を表示（余白可能）
            // display の物理アスペクト比に合わせた ROI を W x H キャンバス上に作り、
            // その ROI 内に動画をアスペクト比を保って FIT して中央に貼り付ける。
            const double display_aspect = (config.total_width * CHAR_WIDTH_MM) / (config.total_height * CHAR_HEIGHT_MM);

            // W x H のキャンバス
            cv::Mat fit_canvas = cv::Mat::zeros(H, W, frame.type());

            // キャンバス上の ROI を決定
            int roi_w, roi_h, roi_x, roi_y;
            if (static_cast<double>(W) / static_cast<double>(H) > display_aspect) {
                // キャンバスは表示より横長 -> ROI の高さを H に合わせる
                roi_h = H;
                roi_w = std::max(1, static_cast<int>(std::round(roi_h * display_aspect)));
                roi_x = (W - roi_w) / 2;
                roi_y = 0;
            } else {
                // キャンバスは表示より縦長 -> ROI の幅を W に合わせる
                roi_w = W;
                roi_h = std::max(1, static_cast<int>(std::round(roi_w / display_aspect)));
                roi_x = 0;
                roi_y = (H - roi_h) / 2;
            }

            // ROI 内に動画を FIT（縮小）して中央寄せ
            double scale2 = std::min(static_cast<double>(roi_w) / frame.cols, static_cast<double>(roi_h) / frame.rows);
            int dst_w = std::max(1, static_cast<int>(frame.cols * scale2 + 0.5));
            int dst_h = std::max(1, static_cast<int>(frame.rows * scale2 + 0.5));
            if (debug) std::cerr << "[FIT-emu] source_aspect=" << source_aspect << " display_aspect=" << display_aspect
                      << " roi=("<<roi_x<<","<<roi_y<<","<<roi_w<<","<<roi_h<<") dst=("<<dst_w<<","<<dst_h<<")\n";
            if (debug) std::cerr << "[FIT] source_aspect=" << source_aspect << " display_aspect=" << display_aspect
                      << " roi=("<<roi_x<<","<<roi_y<<","<<roi_w<<","<<roi_h<<") dst=("<<dst_w<<","<<dst_h<<")\n";
            cv::Mat scaled_frame;
            cv::resize(frame, scaled_frame, cv::Size(dst_w, dst_h));
            int paste_x = roi_x + (roi_w - dst_w) / 2;
            int paste_y = roi_y + (roi_h - dst_h) / 2;
            scaled_frame.copyTo(fit_canvas(cv::Rect(paste_x, paste_y, dst_w, dst_h)));

            // 重要: ROI 部分だけを抜き出して渡す。これで frame_to_grid が期待通りの領域を扱う。
            cropped_frame = fit_canvas(cv::Rect(roi_x, roi_y, roi_w, roi_h)).clone();
        }

        cv::Mat resized_frame, gray_frame, bw_frame;
        // エミュレータ側も同様: FIT と CROP の場合は ROI のまま渡す
        if (scaling_mode == ScalingMode::FIT || scaling_mode == ScalingMode::CROP || scaling_mode == ScalingMode::STRETCH) {
            resized_frame = cropped_frame;
        } else {
            cv::resize(cropped_frame, resized_frame, cv::Size(W, H));
        }
        cv::cvtColor(resized_frame, gray_frame, cv::COLOR_BGR2GRAY);
        cv::threshold(gray_frame, bw_frame, min_threshold, max_threshold, cv::THRESH_BINARY);

        std::vector<uint8_t> grid;
        frame_to_grid(bw_frame, config, grid);

        // エミュレータ表示 (キャッシュされたレイアウトを使用)
        cv::Mat display_frame = cv::Mat::zeros(cache.window_height, cache.window_width, CV_8UC3);

        for (int r = 0; r < config.total_height; ++r) {
            for (int c = 0; c < config.total_width; ++c) {
                int idx = r * config.total_width + c;
                if (idx >= static_cast<int>(grid.size())) continue;
                
                uint8_t seg = grid[idx];
                // キャッシュされたセグメントを使用
                for (int s = 0; s < 7; ++s) {
                    cv::Scalar color = (seg & (1 << s)) ? cv::Scalar(0, 0, 255) : cv::Scalar(80, 80, 80);
                    cv::fillConvexPoly(display_frame, cache.all_segs[idx][s], color, cv::LINE_AA);
                    cv::polylines(display_frame, cache.all_segs[idx][s], true, cv::Scalar(0, 0, 0), 2, cv::LINE_AA);
                }
                // キャッシュされた小数点を使用
                cv::Scalar dp_color = (seg & 0x80) ? cv::Scalar(0, 0, 255) : cv::Scalar(80, 80, 80);
                cv::circle(display_frame, cache.all_dp_centers[idx], cache.all_dp_radii[idx], dp_color, -1, cv::LINE_AA);
                cv::circle(display_frame, cache.all_dp_centers[idx], cache.all_dp_radii[idx], cv::Scalar(0, 0, 0), 2, cv::LINE_AA);
            }
        }
        // エミュレータ表示 (macOSでもGUIウィンドウを表示)
        cv::imshow("7seg-emulator", display_frame);
        if (cv::waitKey(1) == 27) break; // ESCで終了

        // 音声同期: 理想的なフレームタイミングを計算
        frame_count++;
        auto ideal_time = start_time + frame_count * frame_duration;
        auto current_time_frame = std::chrono::steady_clock::now();
        
        if (current_time_frame < ideal_time) {
            // まだ時間が余っている場合は待つ
            std::this_thread::sleep_until(ideal_time);
        } else {
            // 遅れている場合は、次のフレームに進む（ドロップしない）
            next_frame_time = current_time_frame + frame_duration;
        }
    }

    if (video_path != "-") {
        if (use_ffplay) {
            system("killall ffplay > /dev/null 2>&1");
        } else {
            file_audio_stop();
            audio_cleanup();
        }
    }
    cap.release();
    cv::destroyAllWindows();

    // テキストエミュレータの終了メッセージは削除

    if (stop_flag) {
        std::cout << "エミュレータ再生中止: " << video_path << std::endl;
    } else {
        std::cout << "エミュレータ再生終了: " << video_path << std::endl;
    }
    g_current_stop_flag = nullptr;
    return 0;
}

