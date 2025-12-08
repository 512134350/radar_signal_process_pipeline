#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <algorithm>
#include "radar_defines.h"

// 声明顶层函数
void radar_processing_chain(stream_in_t &adc_input, stream_out_t &pc_output);

// 辅助：计算模值 (用于寻找峰值)
double calc_mag(double re, double im) {
    return std::sqrt(re * re + im * im);
}

int main() {
    // ==========================================
    // 1. 仿真参数配置 (需与 Python 脚本一致)
    // ==========================================
    const int N_SAMPLES = 1024;
    const double fs = 100e6;        // 采样率 100MHz
    const double T_pulse = 10.24e-6; // 脉宽 (对应 1024 个点)
    const double B = 20e6;          // 带宽 20MHz
    const double K = B / T_pulse;   // 调频斜率 (Chirp Rate)
    const double SCALE = 0.9;       // 幅度缩放 (防止溢出，与 Python 保持一致)

    stream_in_t input_stream;
    stream_out_t output_stream;

    std::cout << ">> [TB] Starting LFM Pulse Compression Test..." << std::endl;
    std::cout << ">> [TB] Generating LFM Chirp Signal..." << std::endl;

    // ==========================================
    // 2. 生成 LFM Chirp 激励
    //    公式: exp(j * pi * K * t^2)
    // ==========================================
    for (int i = 0; i < N_SAMPLES; i++) {
        axis_in_t tmp_in;

        // 计算时间 t
        double t = (double)i / fs;

        // 计算相位: pi * K * t^2
        double phase = M_PI * K * t * t;

        // 生成复数信号 (实部 cos, 虚部 sin)
        double real_double = SCALE * std::cos(phase);
        double imag_double = SCALE * std::sin(phase);

        // 量化转定点
        fft_data_t real_fx = (fft_data_t)real_double;
        fft_data_t imag_fx = (fft_data_t)imag_double;

        // 打包 (低16位实部，高16位虚部)
        ap_uint<32> packed;
        packed(15, 0)  = real_fx.range(15, 0);
        packed(31, 16) = imag_fx.range(15, 0);

        tmp_in.data = packed;
        tmp_in.last = (i == N_SAMPLES - 1) ? 1 : 0;
        tmp_in.keep = -1;
        tmp_in.strb = -1;

        input_stream.write(tmp_in);
    }

    // ==========================================
    // 3. 调用 DUT (Device Under Test)
    // ==========================================
    std::cout << ">> [TB] Calling DUT..." << std::endl;
    radar_processing_chain(input_stream, output_stream);

    // ==========================================
    // 4. 收集结果并分析
    // ==========================================
    std::cout << ">> [TB] Collecting Results..." << std::endl;
    std::ofstream file("pc_output_lfm.txt");

    int count = 0;
    double max_mag = 0.0;
    int max_idx = -1;

    for (int i = 0; i < N_SAMPLES; i++) {
        axis_out_t temp_out;
        output_stream.read(temp_out);

        // 提取数据 (使用结构体成员 .re 和 .im)
        double re = temp_out.data.re.to_double();
        double im = temp_out.data.im.to_double();
        double mag = calc_mag(re, im);

        // 记录峰值信息
        if (mag > max_mag) {
            max_mag = mag;
            max_idx = i;
        }

        // 保存数据: Index, Real, Imag, Magnitude
        file << i << " " << re << " " << im << " " << mag << "\n";
        count++;
    }

    file.close();

    // ==========================================
    // 5. 自动判定结果
    // ==========================================
    std::cout << ">> [TB] Simulation Finished." << std::endl;
    std::cout << ">> [TB] Max Magnitude: " << max_mag << " at Index: " << max_idx << std::endl;

    // 简单的判断逻辑：
    // 脉冲压缩后，能量应该高度集中。
    // 如果最大值非常小，或者没有明显的峰值，说明失败。
    // 注意：由于是循环卷积，峰值通常出现在索引 0 附近。
    if (max_mag > 0.1 && count == N_SAMPLES) {
        std::cout << ">> [TB] TEST PASS! Pulse Compression Peak Detected." << std::endl;
        std::cout << ">> [Result] Check 'pc_output_lfm.txt' for waveform." << std::endl;
        return 0;
    } else {
        std::cerr << ">> [TB] TEST FAIL! No significant peak detected or sample mismatch." << std::endl;
        return 1;
    }
}
