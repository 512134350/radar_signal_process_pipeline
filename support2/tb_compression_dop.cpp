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

    ofstream file_out("E:/Vitis_Workplace/radar/support/output_dut.dat");



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

        // 假设低 14 位是实部，高 14 位是虚部 (根据你的Python脚本格式)

        // 这里的位操作必须与 radar_top 里的解包逻辑一致

        ap_int<14> r_14 = r;

        ap_int<14> i_14 = i_val;



        pkt.data.range(13, 0) = r_14;

        pkt.data.range(29, 16) = i_14;



        // TLAST 生成逻辑:

        // 这里的雷达顶层是流式处理，通常在整个传输结束或者每个脉冲结束拉高 last

        // 我们在最后一个样本拉高 last

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

    radar_top(input_stream, output_stream);

    cout << "   - DUT execution finished." << endl;



    // ------------------------------------------------------

    // 4. 读取输出并保存到文件

    // ------------------------------------------------------

    cout << ">> [TB] Saving results to output_dut.dat..." << endl;



    int out_cnt = 0;

    double max_mag = 0.0;

    int max_idx_global = -1;



    // radar_top 输出的是距离-多普勒矩阵 (按列输出)

    // Range 0: [Pulse 0...127], Range 1: [Pulse 0...127]...

    // 或者按你的设计：每个 Range Bin 输出所有 Doppler Bin



    while (!output_stream.empty()) {

        axis_out_t out_pkt = output_stream.read();



        // 获取复数结果

        double re = out_pkt.data.re.to_double();

        double im = out_pkt.data.im.to_double();



        // 写入文件 (格式: 实部 虚部)

        file_out << re << " " << im << endl;



        // 统计峰值 (简单的控制台反馈)

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



    // 计算峰值坐标 (假设输出顺序是 Range-major: R0[D0..Dn], R1[D0..Dn]...)

    // 根据 radar_top 代码逻辑：

    // Outer Loop: Range (0~127)

    // Inner Loop: Pulse/Doppler (0~127)

    if (max_idx_global >= 0) {

        int peak_range = max_idx_global / N_PULSE;

        int peak_doppler = max_idx_global % N_PULSE;

        cout << "   - Peak Location -> Range Bin: " << peak_range

             << ", Doppler Bin: " << peak_doppler << endl;

    } else {

        cout << "   - WARNING: No output data found!" << endl;

        return 1;

    }

    cout << "---------------------------------------------" << endl;

    cout << ">> [PASS] Testbench finished. Please run Python script to verify correctness." << endl;



    return 0;

}