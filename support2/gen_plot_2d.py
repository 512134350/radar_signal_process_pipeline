import numpy as np
import matplotlib.pyplot as plt

# 配置必须与 HLS 一致
N_RANGE = 128
N_PULSE = 128

# 之前生成的真值，用于图表标题验证
EXPECTED_RANGE = 50
EXPECTED_DOPPLER = 32

def plot_2d_rd_map():
    print("正在读取 output_dut.dat ...")
    try:
        # 1. 读取数据
        raw_data = np.loadtxt("output_dut.dat")
        if raw_data.shape[0] != N_RANGE * N_PULSE:
            print(f"错误: 数据量不匹配! 期望 {N_RANGE*N_PULSE}, 实际 {raw_data.shape[0]}")
            return
        
        complex_data = raw_data[:, 0] + 1j * raw_data[:, 1]
    except Exception as e:
        print(f"读取文件失败: {e}")
        return

    # 2. 重塑矩阵 (Reshape)
    # 这里的顺序极其重要！必须与 radar_top 的输出顺序一致。
    # radar_top 的输出逻辑是:
    # Outer Loop: Range (0 -> 127)
    #   Inner Loop: Pulse/Doppler (0 -> 127)
    # 所以数据流是 [Range0_DopAll, Range1_DopAll, ...]
    # Python reshape 默认是 C-order (行优先)，所以我们 reshape 成 (N_RANGE, N_PULSE)
    rd_matrix = complex_data.reshape((N_RANGE, N_PULSE))

    # 3. 计算幅度谱 (dB)
    abs_matrix = np.abs(rd_matrix)
    # 加个小值防止 log(0)
    db_matrix = 20 * np.log10(abs_matrix + 1e-9)

    # 4. 寻找峰值
    max_idx = np.unravel_index(np.argmax(abs_matrix), abs_matrix.shape)
    peak_range = max_idx[0]
    peak_doppler = max_idx[1]
    
    print(f"--------------------------------")
    print(f"检测结果:")
    print(f"峰值位置: Range Bin [{peak_range}], Doppler Bin [{peak_doppler}]")
    print(f"预期位置: Range Bin [{EXPECTED_RANGE}], Doppler Bin [{EXPECTED_DOPPLER}]")
    
    if peak_range == EXPECTED_RANGE and peak_doppler == EXPECTED_DOPPLER:
        print(">> 结果判定: PASS (位置完全匹配)")
    else:
        print(">> 结果判定: FAIL (位置偏移)")
    print(f"--------------------------------")

    # 5. 绘制 2D 热力图
    plt.figure(figsize=(12, 5))
    
    # 子图 1: 2D 热力图
    plt.subplot(1, 2, 1)
    # extent参数设置坐标轴范围 [x_min, x_max, y_min, y_max] -> [Doppler, Range]
    # 注意：imshow 的原点默认在左上角，Range通常向上增长，origin='lower' 将原点置于左下
    plt.imshow(db_matrix, aspect='auto', cmap='jet', origin='lower',
               extent=[0, N_PULSE, 0, N_RANGE])
    plt.colorbar(label='Amplitude (dB)')
    plt.title(f'Range-Doppler Map (HLS Output)\nPeak @ R:{peak_range}, D:{peak_doppler}')
    plt.xlabel('Doppler Bin (Velocity)')
    plt.ylabel('Range Bin (Distance)')
    
    # 标记峰值点
    plt.scatter(peak_doppler, peak_range, color='black', marker='x', s=100, label='Detected Peak')
    plt.legend()

    # 子图 2: 峰值切面图 (Range Profile & Doppler Profile)
    plt.subplot(2, 2, 2)
    plt.plot(db_matrix[:, peak_doppler], 'b-')
    plt.title(f'Range Profile (at Doppler {peak_doppler})')
    plt.xlabel('Range Bin')
    plt.ylabel('dB')
    plt.grid(True)

    plt.subplot(2, 2, 4)
    plt.plot(db_matrix[peak_range, :], 'r-')
    plt.title(f'Doppler Profile (at Range {peak_range})')
    plt.xlabel('Doppler Bin')
    plt.ylabel('dB')
    plt.grid(True)

    plt.tight_layout()
    plt.savefig("report_rd_map.png") # 保存图片用于报告
    print("图表已保存为 report_rd_map.png")
    plt.show()

if __name__ == "__main__":
    plot_2d_rd_map()