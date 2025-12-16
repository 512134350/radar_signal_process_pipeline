#include "radar_defines.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <iomanip>

using namespace std;

// =========================================================
// 基于文件的正规 Testbench (多帧循环版)
// 功能：
// 1. 读取输入文件一次，存入内存 Buffer
// 2. 循环运行 3 帧数据，以修复 Co-Sim 报告中的 Latency Fail 问题
// 3. 将所有帧的输出结果连续写入 output_dut.dat
// =========================================================

int main() {
    // ------------------------------------------------------
    // 1. 文件与流配置
    // ------------------------------------------------------
    ifstream file_in("input_stimulus.dat");
    ofstream file_out("output_dut.dat");

    if (!file_in.is_open()) {
        cout << "ERROR: Cannot open input_stimulus.dat!" << endl;
        cout << "Please run the Python script to generate the stimulus file first." << endl;
        return 1;
    }
    if (!file_out.is_open()) {
        cout << "ERROR: Cannot create output_dut.dat!" << endl;
        return 1;
    }

    // 声明流
    stream_in_t input_stream("input_stream");
    stream_out_t output_stream("output_stream");

    // 声明调试计数器变量 (用于监控死锁情况)
    ap_uint<32> debug_in_cnt = 0;
    ap_uint<32> debug_out_cnt = 0;

    cout << ">> [TB] Starting Loop-Based Verification..." << endl;

    // ------------------------------------------------------
    // 2. 预读取输入数据到内存 (只读一次)
    // ------------------------------------------------------
    struct DataPoint { int re; int im; };
    vector<DataPoint> data_buffer;
    int re_in, im_in;

    while (file_in >> re_in >> im_in) {
        data_buffer.push_back({re_in, im_in});
    }
    file_in.close(); // 数据读完了就可以关掉输入文件了

    // 检查数据量
    int samples_per_frame = N_PULSE * N_RANGE;
    if (data_buffer.size() < samples_per_frame) {
        cout << "WARNING: Input file has fewer samples than expected!" << endl;
    }

    // ------------------------------------------------------
    // 3. 多帧循环执行 (关键修改)
    // ------------------------------------------------------
    // 运行 3 帧，让 HLS 能够计算 II (Initiation Interval)
    const int NUM_FRAMES = 3;

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        cout << "---------------------------------------------" << endl;
        cout << ">> [TB] Processing Frame " << frame << " / " << NUM_FRAMES - 1 << "..." << endl;

        // --- Step A: 注入一帧数据 ---
        for (int i = 0; i < samples_per_frame; i++) {
            // 从 buffer 获取数据 (如果不够就补0)
            int r = (i < data_buffer.size()) ? data_buffer[i].re : 0;
            int i_val = (i < data_buffer.size()) ? data_buffer[i].im : 0;

            axis_in_t pkt;
            pkt.data = 0;

            // 14位数据打包
            ap_int<14> r_14 = r;
            ap_int<14> i_14 = i_val;
            pkt.data.range(13, 0) = r_14;
            pkt.data.range(29, 16) = i_14;

            // TLAST 生成逻辑: 每一帧的最后一个样本拉高
            pkt.last = (i == samples_per_frame - 1) ? 1 : 0;
            pkt.keep = -1;
            pkt.strb = -1;

            input_stream.write(pkt);
        }

        // --- Step B: 调用 DUT ---
        // 注意：debug 计数器会累加，方便观察总进度
        radar_top(input_stream, output_stream, &debug_in_cnt, &debug_out_cnt);

        // --- Step C: 读取并保存输出 ---
        int frame_out_cnt = 0;
        double max_mag = 0.0;
        int max_idx = -1;

        while (!output_stream.empty()) {
            axis_out_t out_pkt = output_stream.read();

            // 写入文件
            double re = out_pkt.data.re.to_double();
            double im = out_pkt.data.im.to_double();
            file_out << re << " " << im << endl;

            // 简单的帧内统计
            double mag = sqrt(re*re + im*im);
            if (mag > max_mag) {
                max_mag = mag;
                max_idx = frame_out_cnt;
            }
            frame_out_cnt++;
        }

        // --- Step D: 打印当前帧状态 ---
        cout << "   - Frame Finished." << endl;
        cout << "   - Output Samples: " << frame_out_cnt << endl;
        cout << "   - Debug Counters (Cumulative): In=" << debug_in_cnt << ", Out=" << debug_out_cnt << endl;

        if (max_idx >= 0) {
            int peak_range = max_idx / N_PULSE;
            int peak_doppler = max_idx % N_PULSE;
            cout << "   - Frame Peak -> Mag: " << max_mag
                 << " @ Range: " << peak_range << ", Doppler: " << peak_doppler << endl;
        } else {
             cout << "   - WARNING: No output for this frame!" << endl;
        }
    }

    file_out.close();

    // ------------------------------------------------------
    // 4. 最终报告
    // ------------------------------------------------------
    cout << "---------------------------------------------" << endl;
    cout << ">> [TB] All frames processed." << endl;

    if (debug_out_cnt > 0) {
        cout << ">> [PASS] Testbench finished successfully." << endl;
    } else {
        cout << ">> [FAIL] No data output detected across all frames!" << endl;
        return 1;
    }

    return 0;
}
