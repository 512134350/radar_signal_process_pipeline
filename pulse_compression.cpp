#include "radar_defines.h"

// ==========================================================================
// 0. 系数定义
// ==========================================================================
#if __has_include("radar_coeffs.h")
    static const complex_coeff_t REF_COEFFS[N_RANGE] = {
        #include "radar_coeffs.h"
    };
#else
    #error "Generate radar_coeffs.h using Python script first!"
#endif

// ==========================================================================
// 辅助函数：Status 垃圾桶
// ==========================================================================
static void status_sink(hls::stream<hls::ip_fft::status_t<fft_config>> &sts_stream) {
    #pragma HLS INLINE off
    hls::ip_fft::status_t<fft_config> dummy;
    sts_stream.read(dummy); // 阻塞读取
}

// ==========================================================================
// 1. 输入转换与量化 (位拷贝修复版)
// ==========================================================================
static void input_adaptor(stream_in_t &in, hls::stream<complex_t> &out) {
    #pragma HLS INLINE off
    for (int i = 0; i < N_RANGE; i++) {
        #pragma HLS PIPELINE II=1

        axis_in_t pkt = in.read();

        // 解析 14位 ADC 数据
        ap_int<14> raw_re = pkt.data.range(13, 0);
        ap_int<14> raw_im = pkt.data.range(29, 16);

        // 【关键修复】使用位拷贝，而非数学除法
        // 整数 8191 (0x1FFF) -> 定点数 0.999 (0x1FFF)
        // 这样既避开了编译报错，又防止了数值饱和归零
        adc_t re_adc;
        adc_t im_adc;

        re_adc.range(13, 0) = raw_re.range(13, 0);
        im_adc.range(13, 0) = raw_im.range(13, 0);

        out.write(complex_t((fft_data_t)re_adc, (fft_data_t)im_adc));
    }
}

// ==========================================================================
// 2. 核心处理 (并行流水线)
// ==========================================================================
static void processing_core(hls::stream<complex_t> &in, hls::stream<complex_t> &out) {
    #pragma HLS INLINE off
    #pragma HLS DATAFLOW

    // 内部流与深度
    hls::stream<complex_t> fft_in, fft_out, mult_out, ifft_out;
    #pragma HLS STREAM variable=fft_in depth=128
    #pragma HLS STREAM variable=fft_out depth=128
    #pragma HLS STREAM variable=mult_out depth=128
    #pragma HLS STREAM variable=ifft_out depth=128

    // 配置与状态流
    hls::stream<hls::ip_fft::config_t<fft_config>> fft_cfg, ifft_cfg;
    hls::stream<hls::ip_fft::status_t<fft_config>> fft_sts, ifft_sts;
    #pragma HLS STREAM variable=fft_sts depth=16
    #pragma HLS STREAM variable=ifft_sts depth=16

    // 写配置
    hls::ip_fft::config_t<fft_config> cfg1, cfg2;
    cfg1.setDir(1); cfg1.setSch(0x6A); fft_cfg.write(cfg1);
    cfg2.setDir(0); cfg2.setSch(0x55); ifft_cfg.write(cfg2);

    // --- Dataflow Stages ---

    // Stage A: 将输入转入 FFT 流
    for(int i=0; i<N_RANGE; i++) {
        #pragma HLS PIPELINE II=1
        fft_in.write(in.read());
    }

    // Stage B: Forward FFT
    hls::fft<fft_config>(fft_in, fft_out, fft_sts, fft_cfg);
    status_sink(fft_sts);

    // Stage C: 频域相乘 (匹配滤波)
    for(int i=0; i<N_RANGE; i++) {
        #pragma HLS PIPELINE II=1
        complex_t val = fft_out.read();
        complex_t res = val * REF_COEFFS[i];
        mult_out.write(res);
    }

    // Stage D: Inverse FFT
    hls::fft<fft_config>(mult_out, ifft_out, ifft_sts, ifft_cfg);
    status_sink(ifft_sts);

    // Stage E: 输出
    for(int i=0; i<N_RANGE; i++) {
        #pragma HLS PIPELINE II=1
        out.write(ifft_out.read());
    }
}

// ==========================================================================
// 3. 输出适配
// ==========================================================================
static void output_adaptor(hls::stream<complex_t> &in, stream_out_t &out) {
    #pragma HLS INLINE off
    for (int i = 0; i < N_RANGE; i++) {
        #pragma HLS PIPELINE II=1
        complex_t val = in.read();

        axis_out_t pkt;
        pkt.data.re = val.real();
        pkt.data.im = val.imag();
        pkt.last = (i == N_RANGE - 1) ? 1 : 0;
        pkt.keep = -1;
        pkt.strb = -1;
        out.write(pkt);
    }
}

// ==========================================================================
// 4. 顶层函数
// ==========================================================================
void pulse_compression(stream_in_t &adc_input, stream_out_t &pc_output) {
    #pragma HLS INTERFACE axis port=adc_input
    #pragma HLS INTERFACE axis port=pc_output
    #pragma HLS INTERFACE ap_ctrl_hs port=return
    #pragma HLS DATAFLOW

    static hls::stream<complex_t> s_in_c;
    static hls::stream<complex_t> s_out_c;
    #pragma HLS STREAM variable=s_in_c depth=128
    #pragma HLS STREAM variable=s_out_c depth=128

    input_adaptor(adc_input, s_in_c);
    processing_core(s_in_c, s_out_c);
    output_adaptor(s_out_c, pc_output);
}
