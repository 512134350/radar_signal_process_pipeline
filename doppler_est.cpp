#include "radar_defines.h"

// 这里的改动非常关键：
// 1. 移除 static (如果有)
// 2. 强制 INLINE OFF，切断 FFT 内部逻辑对上层 Dataflow 的干扰
void doppler_est_top(stream_internal_t &in_stream, stream_internal_t &out_stream) {
    #pragma HLS INLINE off

    // 1. 配置 FFT
    hls::ip_fft::config_t<doppler_fft_config> fft_cfg;
    fft_cfg.setDir(1);
    fft_cfg.setSch(0x1555);

    hls::stream<hls::ip_fft::config_t<doppler_fft_config>> config_strm;
    hls::stream<hls::ip_fft::status_t<doppler_fft_config>> status_strm;

    // 【关键】给这两个内部流也加上 depth，防止 FFT 内部卡死
    #pragma HLS STREAM variable=config_strm depth=4
    #pragma HLS STREAM variable=status_strm depth=4

    config_strm.write(fft_cfg);

    // 2. 调用 FFT
    hls::fft<doppler_fft_config>(in_stream, out_stream, status_strm, config_strm);

    // 3. 读状态
    hls::ip_fft::status_t<doppler_fft_config> stat;
    status_strm.read(stat);
}
