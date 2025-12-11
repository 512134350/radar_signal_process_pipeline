#include "radar_defines.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>

using namespace std;

int main() {
    // 1. 打开输入文件
    ifstream file_in("input_stimulus.dat");
    if (!file_in.is_open()) {
        cout << "ERROR: Cannot open input_stimulus.dat. Run Python script first!" << endl;
        return 1;
    }

    // 2. 打开输出文件
    ofstream file_out("output_dut.dat");
    if (!file_out.is_open()) {
        cout << "ERROR: Cannot create output_dut.dat" << endl;
        return 1;
    }

    cout << ">> [TB] Starting Formal Verification..." << endl;

    stream_in_t strm_in;
    stream_out_t strm_out;

    // 3. 读取文件并注入流 (模拟 128 个脉冲)
    // 题目要求流水线处理，我们连续注入数据测试吞吐量

    cout << ">> [TB] Reading file and pushing to DUT..." << endl;

    // 预加载数据到内存防止IO变慢影响仿真
    struct DataPoint { int re; int im; };
    vector<DataPoint> all_data;
    int re_in, im_in;
    while (file_in >> re_in >> im_in) {
        all_data.push_back({re_in, im_in});
    }

    // 将数据转化为 AXI Stream 包
    for (size_t k = 0; k < all_data.size(); k++) {
        axis_in_t pkt;
        // 组装 32位 数据: [29:16]虚部, [13:0]实部
        // 注意掩码操作
        ap_int<14> r = all_data[k].re;
        ap_int<14> i = all_data[k].im;

        pkt.data = 0; // 清零
        pkt.data.range(13, 0) = r;
        pkt.data.range(29, 16) = i;

        pkt.last = ((k + 1) % N_RANGE == 0) ? 1 : 0;
        pkt.keep = -1;
        pkt.strb = -1;

        strm_in.write(pkt);
    }

    // 4. 调用 DUT
    // 注意：因为 pulse_compression 是处理单脉冲(128点)的
    // 我们需要循环调用 N_PULSE 次
    cout << ">> [TB] Running DUT for " << N_PULSE << " pulses..." << endl;

    for (int p = 0; p < N_PULSE; p++) {
        pulse_compression(strm_in, strm_out);
    }

    // 5. 保存结果
    cout << ">> [TB] Saving results..." << endl;
    int sample_count = 0;
    while (!strm_out.empty()) {
        axis_out_t pkt = strm_out.read();

        // 保存为浮点数方便 Python 绘图
        double re = pkt.data.re.to_double();
        double im = pkt.data.im.to_double();

        file_out << re << " " << im << endl;
        sample_count++;
    }

    file_in.close();
    file_out.close();

    cout << ">> [TB] Simulation Complete. " << sample_count << " samples processed." << endl;
    cout << ">> [TB] Please run plot_result.py to verify waveforms." << endl;

    return 0;
}
