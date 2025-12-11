#ifndef RADAR_DEFINES_H
#define RADAR_DEFINES_H

#include <hls_stream.h>
#include <ap_fixed.h>
#include <ap_int.h>
#include <complex>
#include <hls_fft.h>

// ==========================================
// 1. 系统参数
// ==========================================
#define N_RANGE 128
#define N_PULSE 128

// ==========================================
// 2. 数据类型定义
// ==========================================

// A. 输入数据: 14位 ADC (保持 RND/SAT 用于输入转换是没问题的)
typedef ap_fixed<14, 1, AP_RND, AP_SAT> adc_t;

// B. 内部处理与输出: 16位 定点
// 【关键修复】FFT 接口必须使用标准 ap_fixed (无额外参数)
// 否则 hls::fft 模板匹配会失败
typedef ap_fixed<16, 1> fft_data_t;

typedef std::complex<fft_data_t> complex_t;

// C. 系数类型 (可以保持优化模式，用于乘法)
typedef ap_fixed<16, 1, AP_RND, AP_SAT> coeff_t;
//typedef std::complex<coeff_t> complex_coeff_t;
// 为避免类型转换麻烦，系数最好也统一，或者在乘法时强转
typedef std::complex<fft_data_t> complex_coeff_t;

// 结构体用于 Stream
struct my_complex_t {
    fft_data_t re;
    fft_data_t im;
};

// ==========================================
// 3. FFT 配置 (128点)
// ==========================================
struct fft_config : hls::ip_fft::params_t {
    static const unsigned input_width  = 16;
    static const unsigned output_width = 16;
    static const unsigned max_nfft = 7;      // 2^7 = 128
    static const unsigned nfft = 128;
    static const bool     has_nfft = false;
    static const unsigned config_width = 16;
    static const unsigned status_width = 8;
    static const unsigned ordering_opt = hls::ip_fft::natural_order;
    static const unsigned arch_opt = hls::ip_fft::pipelined_streaming_io;
    static const unsigned round_opt = hls::ip_fft::truncation; // 内部截断模式
    static const unsigned scaling_opt = hls::ip_fft::scaled;
};

// (B) 多普勒用的 FFT 配置 (128点)
struct doppler_fft_config : hls::ip_fft::params_t {
    static const unsigned input_width  = 16;
    static const unsigned output_width = 16;
    static const unsigned max_nfft = 7;
    static const unsigned nfft = 128;
    static const bool     has_nfft = false;
    static const unsigned config_width = 16;
    static const unsigned status_width = 8;
    static const unsigned ordering_opt = hls::ip_fft::natural_order;
    static const unsigned arch_opt = hls::ip_fft::pipelined_streaming_io;
    static const unsigned round_opt = hls::ip_fft::truncation;
    static const unsigned scaling_opt = hls::ip_fft::scaled;
};

// ==========================================
// 4. 接口定义
// ==========================================
struct axis_in_t {
    ap_uint<32> data;
    ap_uint<1> last;
    ap_uint<4> keep;
    ap_uint<4> strb;
};

struct axis_out_t {
    my_complex_t data;
    ap_uint<1> last;
    ap_uint<4> keep;
    ap_uint<4> strb;
};

typedef hls::stream<axis_in_t>  stream_in_t;
typedef hls::stream<axis_out_t> stream_out_t;
typedef hls::stream<my_complex_t> stream_mid_t;
typedef hls::stream<complex_t> stream_internal_t; // 补充定义

// 函数声明
void pulse_compression(stream_in_t &adc_input, stream_out_t &pc_output);
void doppler_est_top(stream_internal_t &in_stream, stream_internal_t &out_stream); // 补充声明
void radar_top(stream_in_t &input, stream_out_t &output);

#endif
