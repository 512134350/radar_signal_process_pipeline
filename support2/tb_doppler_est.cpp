#include <iostream>
#include <vector>
#include <complex>
#include <cmath>
#include "hls_stream.h"
#include "ap_fixed.h"

// 声明被测函数
typedef ap_fixed<16, 1> data_real_t;
typedef std::complex<data_real_t> complex_t;

void doppler_est_top(hls::stream<complex_t> &in_stream, float &estimated_freq);

// 对应源码中的参数
const int FFT_LENGTH = 1024;
const float SAMPLE_RATE = 1000000.0f; // 1MHz
const float FREQ_RES = SAMPLE_RATE / FFT_LENGTH;

int main() {
    std::cout << ">> [TB] Starting Doppler Estimation Test..." << std::endl;

    // 1. 设置测试参数
    // 我们设置一个目标多普勒频率，例如 +50 kHz
    // 你可以修改这个值测试负频率，例如 -25000.0f
    float target_doppler_freq = 50000.0f;

    std::cout << ">> [TB] Target Doppler Freq: " << target_doppler_freq << " Hz" << std::endl;
    std::cout << ">> [TB] Frequency Resolution: " << FREQ_RES << " Hz" << std::endl;

    // 2. 生成输入信号 (复数正弦波: exp(j*2*pi*f*t))
    hls::stream<complex_t> in_stream;

    for (int i = 0; i < FFT_LENGTH; i++) {
        float t = (float)i / SAMPLE_RATE;
        float phase = 2.0f * M_PI * target_doppler_freq * t;

        // 生成实部和虚部
        float real_part = 0.5f * cos(phase); // 幅度设为 0.5 避免定点溢出
        float imag_part = 0.5f * sin(phase);

        complex_t sample;
        sample.real(real_part);
        sample.imag(imag_part);

        in_stream.write(sample);
    }

    // 3. 调用 DUT (Device Under Test)
    float est_freq;
    std::cout << ">> [TB] Calling DUT..." << std::endl;
    doppler_est_top(in_stream, est_freq);

    // 4. 验证结果
    std::cout << ">> [TB] Estimated Freq: " << est_freq << " Hz" << std::endl;

    // 计算误差
    float error = std::abs(est_freq - target_doppler_freq);

    // 判断是否通过：误差应该小于一个频率分辨率
    if (error <= FREQ_RES) {
        std::cout << ">> [TB] TEST PASS! Error is within resolution." << std::endl;
        return 0;
    } else {
        std::cout << ">> [TB] TEST FAIL! Error is too large (" << error << " Hz)." << std::endl;
        return 1;
    }
}
