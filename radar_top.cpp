#include "radar_defines.h"
#include <cstdio>

// =========================================================
// [Phase 1 Helper] 将脉压结果存入矩阵 (消费者)
// =========================================================
static void store_pulse_to_matrix(stream_out_t &in_stream,
                                  complex_t matrix[N_PULSE][N_RANGE],
                                  int pulse_idx) {
    #pragma HLS INLINE off
    for (int r = 0; r < N_RANGE; r++) {
        #pragma HLS PIPELINE II=1
        axis_out_t val_pkt = in_stream.read(); // 阻塞读取，有数据就拿走
        complex_t c_val;
        c_val.real(val_pkt.data.re);
        c_val.imag(val_pkt.data.im);
        matrix[pulse_idx][r] = c_val;
    }
}

// =========================================================
// [Phase 1 Logic] 单脉冲处理 Dataflow 封装
// 作用：让 pulse_compression (生产) 和 store (消费) 并行运行
// 彻底消除 FIFO 满导致的死锁
// =========================================================
static void process_single_pulse(stream_in_t &input,
                                 complex_t matrix[N_PULSE][N_RANGE],
                                 int pulse_idx) {
    #pragma HLS INLINE off
    #pragma HLS DATAFLOW // <--- 关键！开启并行

    // 局部流：连接脉压和存储
    // 因为是并行读写，深度设为 16 就极其安全，不需要 128 了
    stream_out_t pc_out_stream;
    #pragma HLS STREAM variable=pc_out_stream depth=16 type=fifo

    // 任务 A: 脉冲压缩 (生产者)
    pulse_compression(input, pc_out_stream);

    // 任务 B: 存入矩阵 (消费者)
    store_pulse_to_matrix(pc_out_stream, matrix, pulse_idx);
}


// =========================================================
// [Phase 2 Helper] RAM -> Stream (带调试计数)
// =========================================================
static void load_buff_to_stream(complex_t buff[N_PULSE],
                                hls::stream<complex_t> &out_strm,
                                ap_uint<32> &dbg_cnt) {
    #pragma HLS INLINE off
    for (int i = 0; i < N_PULSE; i++) {
        #pragma HLS PIPELINE II=1
        out_strm.write(buff[i]);
        dbg_cnt++;
    }
}

// =========================================================
// [Phase 2 Helper] Stream -> RAM (带调试计数)
// =========================================================
static void store_stream_to_buff(hls::stream<complex_t> &in_strm,
                                 complex_t buff[N_PULSE],
                                 ap_uint<32> &dbg_cnt) {
    #pragma HLS INLINE off
    for (int i = 0; i < N_PULSE; i++) {
        #pragma HLS PIPELINE II=1
        buff[i] = in_strm.read();
        dbg_cnt++;
    }
}

// =========================================================
// [Phase 2 Logic] 单列处理 Dataflow 函数 (PIPO 方案)
// =========================================================
static void process_single_column(complex_t mem_matrix[N_PULSE][N_RANGE],
                                  stream_out_t &output,
                                  int r,
                                  ap_uint<32> &dbg_in,
                                  ap_uint<32> &dbg_out) {
    #pragma HLS INLINE off
    #pragma HLS DATAFLOW

    // 1. 物理存储 RAM (Ping-Pong)
    complex_t buff_in[N_PULSE];
    complex_t buff_out[N_PULSE];
    #pragma HLS BIND_STORAGE variable=buff_in type=ram_2p impl=bram
    #pragma HLS BIND_STORAGE variable=buff_out type=ram_2p impl=bram

    // 2. 连接 FFT 的内部流 (给足 128 深度以防万一)
    hls::stream<complex_t> fft_in_strm;
    hls::stream<complex_t> fft_out_strm;
    #pragma HLS STREAM variable=fft_in_strm  depth=128 type=fifo
    #pragma HLS STREAM variable=fft_out_strm depth=128 type=fifo

    // Stage A: Matrix -> Buffer
    for (int p = 0; p < N_PULSE; p++) {
        #pragma HLS PIPELINE II=1
        buff_in[p] = mem_matrix[p][r];
    }

    // Stage B: Buffer -> Stream (埋点)
    load_buff_to_stream(buff_in, fft_in_strm, dbg_in);

    // Stage C: FFT Core
    doppler_est_top(fft_in_strm, fft_out_strm);

    // Stage D: Stream -> Buffer (埋点)
    store_stream_to_buff(fft_out_strm, buff_out, dbg_out);

    // Stage E: Buffer -> Output
    for (int p = 0; p < N_PULSE; p++) {
        #pragma HLS PIPELINE II=1
        complex_t val = buff_out[p];
        axis_out_t out_pkt;
        out_pkt.data.re = val.real();
        out_pkt.data.im = val.imag();
        out_pkt.last = (r == N_RANGE - 1) && (p == N_PULSE - 1);
        out_pkt.keep = -1;
        out_pkt.strb = -1;
        output.write(out_pkt);
    }
}

// =========================================================
// Phase 2 循环驱动
// =========================================================
static void run_phase2_doppler(complex_t mem_matrix[N_PULSE][N_RANGE],
                               stream_out_t &output,
                               ap_uint<32> &dbg_in,
                               ap_uint<32> &dbg_out) {
    #pragma HLS INLINE off
    Doppler_Outer_Loop: for (int r = 0; r < N_RANGE; r++) {
        process_single_column(mem_matrix, output, r, dbg_in, dbg_out);
    }
}

// =========================================================
// 顶层函数
// =========================================================
void radar_top(stream_in_t &input,
               stream_out_t &output,
               ap_uint<32> *dbg_fft_in_cnt,
               ap_uint<32> *dbg_fft_out_cnt)
{
    #pragma HLS INTERFACE axis port=input
    #pragma HLS INTERFACE axis port=output
    #pragma HLS INTERFACE ap_none port=dbg_fft_in_cnt
    #pragma HLS INTERFACE ap_none port=dbg_fft_out_cnt
    #pragma HLS INTERFACE ap_ctrl_hs port=return

    ap_uint<32> d_in = 0;
    ap_uint<32> d_out = 0;

    static complex_t mem_matrix[N_PULSE][N_RANGE];
    #pragma HLS RESOURCE variable=mem_matrix core=RAM_2P_BRAM
    #pragma HLS ARRAY_PARTITION variable=mem_matrix cyclic factor=4 dim=2

    printf(">> [DUT] Phase 1 Start (Dataflow)...\n");

    // 【修改】Phase 1 循环：现在调用 Dataflow 封装函数
    Pulse_Loop: for (int p = 0; p < N_PULSE; p++) {
        // 这会让脉压和存储同时进行，不会因为 FIFO 满而死锁
        process_single_pulse(input, mem_matrix, p);
    }

    printf(">> [DUT] Phase 1 Complete.\n");

    printf(">> [DUT] Phase 2 Start (Dataflow)...\n");

    // Phase 2
    run_phase2_doppler(mem_matrix, output, d_in, d_out);

    *dbg_fft_in_cnt = d_in;
    *dbg_fft_out_cnt = d_out;

    printf(">> [DUT] Phase 2 Complete.\n");
}
