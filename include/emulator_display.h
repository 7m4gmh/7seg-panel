// emulator_display.h
#pragma once
#include <vector>
#include <cstdint>

// 7segエミュレータ/物理LED共通の出力インターフェース
class IDisplayOutput {
public:
    virtual ~IDisplayOutput() = default;
    virtual void update(const std::vector<uint8_t>& grid) = 0;
};

// デバッグモードフラグ
extern bool debug_mode;

// エミュレータ用出力クラス生成
IDisplayOutput* create_emulator_display(int digits);
IDisplayOutput* create_emulator_display(int rows, int cols);
IDisplayOutput* create_emulator_display(const std::string& config_name);
