#ifndef RADAR_DEFINES_H
#define RADAR_DEFINES_H

#include <hls_stream.h>
#include <ap_fixed.h>
#include <ap_int.h>
#include <complex>
#include <hls_fft.h>

#define N_RANGE 1024
#define N_PULSE 1

// FFT 配置 (恢复 arch_opt)
struct fft_config : hls::ip_fft::params_t {
    static const unsigned input_width  = 16;
    static const unsigned output_width = 16;
    static const unsigned max_nfft = 10;
    static const unsigned nfft = 1024;
    static const bool     has_nfft = false;
    static const unsigned config_width = 16;
    static const unsigned status_width = 8;
    static const unsigned ordering_opt = hls::ip_fft::natural_order;
    static const unsigned arch_opt = hls::ip_fft::pipelined_streaming_io; // 必须显式开启
    static const unsigned round_opt = hls::ip_fft::truncation;
    static const unsigned scaling_opt = hls::ip_fft::scaled;
};

constexpr unsigned FFT_ALIGNED_WIDTH = ((fft_config::input_width + 7) / 8) * 8;
typedef ap_fixed<FFT_ALIGNED_WIDTH, 1> fft_data_t;
typedef std::complex<fft_data_t> complex_t; // 仅用于 FFT 核心计算

// 【新增】传输专用结构体 (短名字，防止 Verilog 端口名爆炸)
struct my_complex_t {
    fft_data_t re;
    fft_data_t im;
};

// 顶层接口结构体
struct axis_in_t {
    ap_uint<32> data;
    ap_uint<1> last;
    ap_uint<4> keep;
    ap_uint<4> strb;
};

struct axis_out_t {
    my_complex_t data; // 注意：输出也用这个短结构体，最后再组装
    ap_uint<1> last;
    ap_uint<4> keep;
    ap_uint<4> strb;
};

// 流定义
typedef hls::stream<axis_in_t>  stream_in_t;
typedef hls::stream<axis_out_t> stream_out_t;
typedef hls::stream<my_complex_t> stream_mid_t; // 中间流使用短结构体

#endif
