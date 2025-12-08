#include "radar_defines.h"

// 引入 Python 生成的系数文件
// 必须确保 radar_coeffs.h 在工程目录下
static const complex_t REF_COEFFS[N_RANGE] = {
    #include "radar_coeffs.h"
};

// ==========================================================================
// 1. 输入适配模块
// ==========================================================================
static void convert_axis_to_complex(stream_in_t &in, stream_mid_t &out) {
    #pragma HLS INLINE off
    for (int p = 0; p < N_PULSE; p++) {
        for (int r = 0; r < N_RANGE; r++) {
            #pragma HLS PIPELINE II=1
            axis_in_t temp_in;
            in.read(temp_in);

            ap_uint<32> raw_data = temp_in.data;
            my_complex_t val;
            val.re.range(15, 0) = raw_data(15, 0);
            val.im.range(15, 0) = raw_data(31, 16);

            out.write(val);
        }
    }
}

// ==========================================================================
// 2. 脉冲压缩核心链 (FFT -> Mult -> IFFT)
// ==========================================================================
static void run_pc_chain(stream_mid_t &in, stream_mid_t &out) {
    #pragma HLS INLINE off
    #pragma HLS DATAFLOW

    // 强制接口为 FIFO (规避长命名报错)
    #pragma HLS INTERFACE ap_fifo port=in
    #pragma HLS INTERFACE ap_fifo port=out
    #pragma HLS INTERFACE ap_ctrl_none port=return

    // --- 内部流声明 ---
    // 1. FFT 输入
    hls::stream<complex_t> fft_in_c("fft_in_c");
    // 2. FFT 输出 (也是乘法输入)
    hls::stream<complex_t> fft_out_c("fft_out_c");
    // 3. 乘法输出 (也是 IFFT 输入)
    hls::stream<complex_t> mult_out_c("mult_out_c");
    // 4. IFFT 输出
    hls::stream<complex_t> ifft_out_c("ifft_out_c");

    // 设置深度 (防止流水线阻塞)
    #pragma HLS STREAM variable=fft_in_c depth=1024
    #pragma HLS STREAM variable=fft_out_c depth=1024
    #pragma HLS STREAM variable=mult_out_c depth=1024
    #pragma HLS STREAM variable=ifft_out_c depth=1024

    // --- 配置流声明 ---
    hls::stream<hls::ip_fft::config_t<fft_config>> fft_cfg_strm;
    hls::stream<hls::ip_fft::status_t<fft_config>> fft_sts_strm;
    hls::stream<hls::ip_fft::config_t<fft_config>> ifft_cfg_strm;
    hls::stream<hls::ip_fft::status_t<fft_config>> ifft_sts_strm;
    #pragma HLS STREAM variable=fft_cfg_strm depth=4
    #pragma HLS STREAM variable=fft_sts_strm depth=4
    #pragma HLS STREAM variable=ifft_cfg_strm depth=4
    #pragma HLS STREAM variable=ifft_sts_strm depth=4

    // --- 初始化配置 ---
    // 1. 配置 Forward FFT (Dir = 1)
    hls::ip_fft::config_t<fft_config> fft_cfg;
    fft_cfg.setDir(1);
    fft_cfg.setSch(0x2AB); // 缩放因子 (1010101011)
    fft_cfg_strm.write(fft_cfg);

    // 2. 配置 Inverse FFT (Dir = 0)
    hls::ip_fft::config_t<fft_config> ifft_cfg;
    ifft_cfg.setDir(0); // IFFT
    ifft_cfg.setSch(0x001); // IFFT 也需要缩放，防止位宽溢出

    ifft_cfg_strm.write(ifft_cfg);

    // ======================================================
    // Stage 1: 数据转换 (Struct -> Complex)
    // ======================================================
    for (int i = 0; i < N_PULSE * N_RANGE; i++) {
        #pragma HLS PIPELINE II=1
        my_complex_t tmp = in.read();
        fft_in_c.write(complex_t(tmp.re, tmp.im));
    }

    // ======================================================
    // Stage 2: 执行 Forward FFT
    // ======================================================
    hls::fft<fft_config>(fft_in_c, fft_out_c, fft_sts_strm, fft_cfg_strm);

    // 消费状态流 (必须)
    hls::ip_fft::status_t<fft_config> s1;
    if(!fft_sts_strm.empty()) fft_sts_strm.read(s1);

    // ======================================================
    // Stage 3: 频域乘法 (Matched Filtering)
    // ======================================================
    for (int p = 0; p < N_PULSE; p++) {
        for (int r = 0; r < N_RANGE; r++) {
            #pragma HLS PIPELINE II=1

            complex_t fft_val = fft_out_c.read();
            complex_t coeff_val = REF_COEFFS[r]; // 读取 ROM 中的系数

            // 复数乘法: (a+bi)(c+di) = (ac-bd) + j(ad+bc)
            // hls::complex 已经重载了 * 运算符，直接乘即可
            // 这里的 coeff 已经是共轭过的，所以直接乘
            complex_t result = fft_val * coeff_val;

            mult_out_c.write(result);
        }
    }

    // ======================================================
    // Stage 4: 执行 Inverse FFT (IFFT)
    // ======================================================
    hls::fft<fft_config>(mult_out_c, ifft_out_c, ifft_sts_strm, ifft_cfg_strm);

    // 消费状态流
    hls::ip_fft::status_t<fft_config> s2;
    if(!ifft_sts_strm.empty()) ifft_sts_strm.read(s2);

    // ======================================================
    // Stage 5: 数据转换 (Complex -> Struct)
    // ======================================================
    for (int i = 0; i < N_PULSE * N_RANGE; i++) {
        #pragma HLS PIPELINE II=1
        complex_t tmp = ifft_out_c.read();
        my_complex_t val;
        val.re = tmp.real();
        val.im = tmp.imag();
        out.write(val);
    }
}

// ==========================================================================
// 3. 输出适配模块
// ==========================================================================
static void convert_complex_to_output(stream_mid_t &in, stream_out_t &out) {
    #pragma HLS INLINE off
    for (int p = 0; p < N_PULSE; p++) {
        for (int r = 0; r < N_RANGE; r++) {
            #pragma HLS PIPELINE II=1

            my_complex_t val;
            in.read(val);

            axis_out_t tmp_out;
            tmp_out.data = val;
            // 生成 TLAST
            tmp_out.last = (r == N_RANGE - 1) ? 1 : 0;
            tmp_out.keep = -1;
            tmp_out.strb = -1;

            out.write(tmp_out);
        }
    }
}

// ==========================================================================
// 4. 顶层函数
// ==========================================================================
void pulse_compression(stream_in_t &adc_input, stream_out_t &pc_output) {
    #pragma HLS INTERFACE axis port=adc_input
    #pragma HLS INTERFACE axis port=pc_output
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    #pragma HLS DATAFLOW

    static hls::stream<my_complex_t> s_pc_in("s_pc_in");
    static hls::stream<my_complex_t> s_pc_out("s_pc_out");
    #pragma HLS STREAM variable=s_pc_in depth=1024
    #pragma HLS STREAM variable=s_pc_out depth=1024

    // 1. 输入转换
    convert_axis_to_complex(adc_input, s_pc_in);

    // 2. 脉冲压缩核心链
    run_pc_chain(s_pc_in, s_pc_out);

    // 3. 输出转换
    convert_complex_to_output(s_pc_out, pc_output);
}
