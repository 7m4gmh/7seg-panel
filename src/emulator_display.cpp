// emulator_display.cpp
// 7セグメントLEDエミュレータ用出力クラス（OpenCV使用例）
#include "emulator_display.h"
#include <opencv2/opencv.hpp>
#include <opencv2/core/ocl.hpp>  // OpenCL/GPUサポート用
#include <vector>
#include <thread>
#include <mutex>
#include <iostream> // デバッグログ出力用
#include <fstream>
#include "json.hpp"

// デバッグモードフラグ
bool debug_mode = false;


// Python版と同じ比率・スケール・オフセットでセグメント座標を計算
namespace {
constexpr int CANVAS_W = 200;
constexpr int CANVAS_H = 300;

constexpr double UNIT_W = 12.7; // 1桁あたりの横幅（物理パッケージ幅）
constexpr double UNIT_H = 19.05; // 1桁あたりの縦幅
constexpr double SEG_L = 6.0;   // 横セグメント長さ
constexpr double SEG_W = 2;   // セグメント幅
constexpr double TILT = 10.0;   // degree
constexpr double X_RANGE = 3.0;
constexpr double Y_RANGE = 4.0;
constexpr double SCALE = 8.0; // スケール値を調整

inline int seg_x(double x) {
    int result = CANVAS_W / 2 + static_cast<int>(x * SCALE);
    if (debug_mode) std::cout << "seg_x(" << x << ") = " << result << std::endl; // デバッグログを追加
    return std::clamp(result, 0, CANVAS_W); // キャンバス範囲内に制限
}

inline int seg_y(double y) {
    int result = CANVAS_H / 2 - static_cast<int>(y * SCALE);
    if (debug_mode) std::cout << "seg_y(" << y << ") = " << result << std::endl; // デバッグログを追加
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

SegmentLayout make_layout(int digit_idx, double package_center_x, double package_center_y) {
    // ...existing code...
    // --- まず座標・補正量をすべて宣言 ---
    double tilt_rad = TILT * CV_PI / 180.0;
    double vlen = SEG_L * SCALE; // 横セグメント長さを縦セグメント高さとみなす
    int tilt_dx = static_cast<int>(vlen/2 * std::tan(tilt_rad) + 0.5);
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
}



class EmulatorDisplay : public IDisplayOutput {
public:
    EmulatorDisplay(int rows, int cols) : rows_(rows), cols_(cols) {
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
        
        gpu_img_ = cv::UMat(1, 1, CV_8UC3); // GPU用画像の仮初期化
        // パッケージ中心座標を全桁分計算
        double digit_spacing_x = UNIT_W * SCALE;
        double digit_spacing_y = UNIT_H * SCALE;
        for (int r = 0; r < rows_; ++r) {
            for (int c = 0; c < cols_; ++c) {
                double package_center_x = c * digit_spacing_x + (digit_spacing_x / 2.0);
                double package_center_y = r * digit_spacing_y + (digit_spacing_y / 2.0);
                package_centers_.emplace_back(package_center_x, package_center_y);
            }
        }
    }
    void update(const std::vector<uint8_t>& grid) override {
        std::lock_guard<std::mutex> lock(mtx_);

        int window_width = static_cast<int>(cols_ * UNIT_W * SCALE);
        int window_height = static_cast<int>(rows_ * UNIT_H * SCALE);
        
        // GPUアクセラレーションを使用（UMat）
        gpu_img_ = cv::UMat::zeros(window_height, window_width, CV_8UC3);
        
        for (int r = 0; r < rows_; ++r) {
            for (int c = 0; c < cols_; ++c) {
                int idx = r * cols_ + c;
                if (idx >= static_cast<int>(grid.size())) continue;
                auto [package_center_x, package_center_y] = package_centers_[idx];
                auto layout = make_layout(0, package_center_x, package_center_y); // digit_idxは未使用なので0
                uint8_t seg = grid[idx];
                for (int s = 0; s < 7; ++s) {
                    cv::Scalar color = (seg & (1 << s)) ? cv::Scalar(0, 0, 255) : cv::Scalar(80, 80, 80);
                    cv::fillConvexPoly(gpu_img_, layout.segs[s], color, cv::LINE_AA);
                    cv::polylines(gpu_img_, layout.segs[s], true, cv::Scalar(0, 0, 0), 2, cv::LINE_AA);
                }
                cv::Scalar dp_color = (seg & 0x80) ? cv::Scalar(0, 0, 255) : cv::Scalar(80, 80, 80);
                cv::circle(gpu_img_, layout.dp_center, layout.dp_radius, dp_color, -1, cv::LINE_AA);
                cv::circle(gpu_img_, layout.dp_center, layout.dp_radius, cv::Scalar(0, 0, 0), 2, cv::LINE_AA);
            }
        }
        
        // 表示用にCPUメモリにコピー（スコープを限定）
        {
            cv::Mat display_img = gpu_img_.getMat(cv::ACCESS_READ);
            cv::imshow("7seg-emulator", display_img);
        }
        cv::waitKey(1);
    }
private:
    int rows_;
    int cols_;
    cv::UMat gpu_img_;  // GPU用画像（処理・表示用）
    std::mutex mtx_;
    std::vector<std::pair<double, double>> package_centers_;
};


IDisplayOutput* create_emulator_display(int rows, int cols) {
    return new EmulatorDisplay(rows, cols);
}

IDisplayOutput* create_emulator_display(const std::string& config_name) {
    std::ifstream f("config.json");
    if (!f.is_open()) {
        std::cerr << "Failed to open config.json" << std::endl;
        return nullptr;
    }
    nlohmann::json config;
    f >> config;
    if (config["configurations"].find(config_name) == config["configurations"].end()) {
        std::cerr << "Configuration " << config_name << " not found" << std::endl;
        return nullptr;
    }
    auto& conf = config["configurations"][config_name];
    int total_width = conf["total_width"];
    int total_height = conf["total_height"];
    return new EmulatorDisplay(total_height, total_width); // rows, cols
}
