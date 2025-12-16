#include "radar_defines.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <iomanip>

using namespace std;

// =========================================================
// 基于文件的正规 Testbench
// 功能：
// 1. 读取 Python 生成的 input_stimulus.dat (14位 ADC数据)
// 2. 将数据打包送入 radar_top
// 3. 将 radar_top 的输出写入 output_dut.dat
// 4. 在控制台打印简单的统计信息 (峰值位置)
// =========================================================

int main() {
    // ------------------------------------------------------
    // 1. 打开文件
    // ------------------------------------------------------
    // 注意：如果在 Co-simulation 中找不到文件，请使用绝对路径
    // 例如: ifstream file_in("E:/Vitis_Workplace/radar/input_stimulus.dat");
    ifstream file_in("input_stimulus.dat");
    ofstream file_out("output_dut.dat"); // 使用相对路径通常即可，若报错改为绝对路径

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

    // 【修改点 1】声明调试计数器变量
    ap_uint<32> debug_in_cnt = 0;
    ap_uint<32> debug_out_cnt = 0;

    cout << ">> [TB] Starting File-Based Verification..." << endl;

    // ------------------------------------------------------
    // 2. 读取输入文件并注入流
    // ------------------------------------------------------
    cout << ">> [TB] Reading input file and pushing to DUT..." << endl;

    int re_in, im_in;
    int sample_count = 0;

    // 临时存储所有数据，确保文件读取和流写入解耦
    struct DataPoint { int re; int im; };
    vector<DataPoint> data_buffer;

    while (file_in >> re_in >> im_in) {
        data_buffer.push_back({re_in, im_in});
    }

    // 检查数据量是否足够
    int total_samples = N_PULSE * N_RANGE;
    if (data_buffer.size() < total_samples) {
        cout << "WARNING: Input file has fewer samples than expected!" << endl;
        cout << "Expected: " << total_samples << ", Found: " << data_buffer.size() << endl;
    }

    // 将数据写入 AXI Stream
    for (int i = 0; i < total_samples; i++) {
        // 防止越界（如果文件数据不够，补0）
        int r = (i < data_buffer.size()) ? data_buffer[i].re : 0;
        int i_val = (i < data_buffer.size()) ? data_buffer[i].im : 0;

        axis_in_t pkt;
        pkt.data = 0;

        // 模拟 14位 ADC 数据打包
        // 假设低 14 位是实部，高 14 位是虚部
        ap_int<14> r_14 = r;
        ap_int<14> i_14 = i_val;

        pkt.data.range(13, 0) = r_14;
        pkt.data.range(29, 16) = i_14;

        // TLAST 生成逻辑:
        // 在最后一个样本拉高 last
        pkt.last = (i == total_samples - 1) ? 1 : 0;
        pkt.keep = -1;
        pkt.strb = -1;

        input_stream.write(pkt);
        sample_count++;
    }
    cout << "   - Pushed " << sample_count << " samples to input stream." << endl;

    // ------------------------------------------------------
    // 3. 调用 DUT
    // ------------------------------------------------------
    cout << ">> [TB] Calling radar_top..." << endl;

    // 【修改点 2】传入调试端口的地址
    radar_top(input_stream, output_stream, &debug_in_cnt, &debug_out_cnt);

    cout << "   - DUT execution finished." << endl;

    // 【修改点 3】打印调试信息
    cout << "   - [DEBUG] FFT In Count: " << debug_in_cnt << endl;
    cout << "   - [DEBUG] FFT Out Count: " << debug_out_cnt << endl;

    // ------------------------------------------------------
    // 4. 读取输出并保存到文件
    // ------------------------------------------------------
    cout << ">> [TB] Saving results to output_dut.dat..." << endl;

    int out_cnt = 0;
    double max_mag = 0.0;
    int max_idx_global = -1;

    // radar_top 输出的是距离-多普勒矩阵 (按列输出)
    while (!output_stream.empty()) {
        axis_out_t out_pkt = output_stream.read();

        // 获取复数结果
        double re = out_pkt.data.re.to_double();
        double im = out_pkt.data.im.to_double();

        // 写入文件 (格式: 实部 虚部)
        file_out << re << " " << im << endl;

        // 统计峰值
        double mag = sqrt(re*re + im*im);
        if (mag > max_mag) {
            max_mag = mag;
            max_idx_global = out_cnt;
        }

        out_cnt++;
    }

    file_in.close();
    file_out.close();

    // ------------------------------------------------------
    // 5. 简单分析报告
    // ------------------------------------------------------
    cout << "---------------------------------------------" << endl;
    cout << ">> [TB] Simulation Summary" << endl;
    cout << "   - Processed Output Samples: " << out_cnt << endl;
    cout << "   - Max Magnitude: " << max_mag << endl;

    if (max_idx_global >= 0) {
        int peak_range = max_idx_global / N_PULSE;
        int peak_doppler = max_idx_global % N_PULSE;
        cout << "   - Peak Location -> Range Bin: " << peak_range
             << ", Doppler Bin: " << peak_doppler << endl;
    } else {
        cout << "   - WARNING: No output data found!" << endl;
        if (debug_out_cnt == 0) {
             cout << "   - [DIAGNOSIS] Deadlock detected! FFT Output Count is 0." << endl;
        }
        return 1;
    }
    cout << "---------------------------------------------" << endl;
    cout << ">> [PASS] Testbench finished." << endl;

    return 0;
}
