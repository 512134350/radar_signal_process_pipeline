#include "radar_defines.h"
#include <cstdio>

void radar_top(stream_in_t &input, stream_out_t &output) {
    #pragma HLS INTERFACE axis port=input
    #pragma HLS INTERFACE axis port=output
    #pragma HLS INTERFACE ap_ctrl_hs port=return

    // 静态存储矩阵
    static complex_t mem_matrix[N_PULSE][N_RANGE];
    #pragma HLS RESOURCE variable=mem_matrix core=RAM_2P_BRAM
    #pragma HLS ARRAY_PARTITION variable=mem_matrix cyclic factor=4 dim=2

     stream_out_t pc_out_stream;
	#pragma HLS STREAM variable=pc_out_stream depth=2048 type=fifo
    #pragma HLS BIND_STORAGE variable=pc_out_stream type=fifo impl=bram

     // 1. 定义 Phase 2 的输入流 (注意：要在 radar_defines.h 里确认 stream_internal_t 是 hls::stream<complex_t>)
     // 如果 typedef 导致构造函数报错，直接用 hls::stream<complex_t>
     hls::stream<complex_t> fft_in_col("fft_in_col", 2048);
     #pragma HLS BIND_STORAGE variable=fft_in_col type=fifo impl=bram

     // 2. 定义 Phase 2 的输出流
     hls::stream<complex_t> fft_out_col("fft_out_col", 2048);
     #pragma HLS BIND_STORAGE variable=fft_out_col type=fifo impl=bram

    // ==============================================================
    // 阶段 1: 脉冲积累
    // ==============================================================
    printf(">> [DUT] Phase 1 Start...\n");

    Pulse_Loop: for (int p = 0; p < N_PULSE; p++) {
        // A. 脉冲压缩
        pulse_compression(input, pc_out_stream);

        // B. 存入矩阵
        Store_Loop: for (int r = 0; r < N_RANGE; r++) {
            #pragma HLS PIPELINE II=1

            // 【修正】直接阻塞读取，去掉 if(empty) return
            axis_out_t val_pkt = pc_out_stream.read();

            complex_t c_val;
            c_val.real(val_pkt.data.re);
            c_val.imag(val_pkt.data.im);
            mem_matrix[p][r] = c_val;
        }
    }
    printf(">> [DUT] Phase 1 Complete.\n");

    // ==============================================================
    // 阶段 2: 多普勒处理
    // ==============================================================


    printf(">> [DUT] Phase 2 Start...\n");

    Doppler_Outer_Loop: for (int r = 0; r < N_RANGE; r++) {
        // A. 读一列
        Col_Read_Loop: for (int p = 0; p < N_PULSE; p++) {
            #pragma HLS PIPELINE II=1
            fft_in_col.write(mem_matrix[p][r]);
        }

        // B. 多普勒 FFT
        doppler_est_top(fft_in_col, fft_out_col);

        // C. 输出结果
        Col_Write_Loop: for (int p = 0; p < N_PULSE; p++) {
            #pragma HLS PIPELINE II=1

            // 【修正】直接阻塞读取，严禁在这里 return！
            complex_t val = fft_out_col.read();

            axis_out_t out_pkt;
            out_pkt.data.re = val.real();
            out_pkt.data.im = val.imag();

            bool is_last = (r == N_RANGE - 1) && (p == N_PULSE - 1);
            out_pkt.last = is_last ? 1 : 0;
            out_pkt.keep = -1;
            out_pkt.strb = -1;

            output.write(out_pkt);
        }
    }
    printf(">> [DUT] Phase 2 Complete.\n");
}
