#include "radar_defines.h"

/**
 * 子模块：多普勒 FFT
 * 作用：对输入的 128 点时域数据做 FFT，得到频域数据
 */
void doppler_est_top(stream_internal_t &in_stream, stream_internal_t &out_stream) {
    #pragma HLS INLINE off // 保持独立层级

    // 1. 配置 FFT 参数
    hls::ip_fft::config_t<doppler_fft_config> fft_cfg;
    fft_cfg.setDir(1);     // Forward FFT

    fft_cfg.setSch(0x1555);// Scaling schedule

    // 2. 【关键修改】使用局部非静态流，确保每次调用都是全新的
    hls::stream<hls::ip_fft::config_t<doppler_fft_config>> config_strm;
    hls::stream<hls::ip_fft::status_t<doppler_fft_config>> status_strm;

    // 写入配置
    config_strm.write(fft_cfg);

    // 3. 执行 FFT
    // 此时输入流应该刚好有 128 个点
    hls::fft<doppler_fft_config>(in_stream, out_stream, status_strm, config_strm);


    // 4. 【修改】去掉 if(!empty)，直接 read() 阻塞等待
        // 必须强制等待 FFT 结束，否则函数会提前返回，导致 FFT 被切断
        hls::ip_fft::status_t<doppler_fft_config> stat;
        status_strm.read(stat);
}
