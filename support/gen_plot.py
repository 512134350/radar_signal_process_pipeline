import numpy as np
import matplotlib.pyplot as plt

N_RANGE = 128

def plot_verification():
    # 1. 读取 HLS 输出
    try:
        # 读取复数数据
        dut_data = np.loadtxt("output_dut.dat")
        dut_complex = dut_data[:, 0] + 1j * dut_data[:, 1]
    except:
        print("未找到 output_dut.dat，请先运行 HLS C Sim/Co-sim")
        return

    # 2. 读取 Golden Reference (可选，这里我们用 DUT 的第一个脉冲做演示)
    # 取第 0 个脉冲的结果进行详细分析
    pulse_0 = dut_complex[0:N_RANGE]
    abs_val = np.abs(pulse_0)
    
    # 3. 绘图 (用于报告)
    plt.figure(figsize=(10, 6))
    
    plt.subplot(2, 1, 1)
    plt.plot(abs_val, 'b.-', label='HLS Output (Fixed Point)')
    plt.title('Pulse Compression Result (Pulse 0)')
    plt.ylabel('Magnitude')
    plt.grid(True)
    plt.legend()
    
    plt.subplot(2, 1, 2)
    # 转成 dB 显示更符合雷达习惯
    db_val = 20 * np.log10(abs_val + 1e-9)
    plt.plot(db_val, 'r.-', label='Log Magnitude')
    plt.title('Log Magnitude (dB)')
    plt.xlabel('Range Bin')
    plt.ylabel('dB')
    plt.grid(True)
    
    plt.tight_layout()
    plt.savefig("report_plot_pc.png") # 保存图片用于写报告
    plt.show()

    # 4. 打印峰值位置
    peak_idx = np.argmax(abs_val)
    print(f"峰值位置: {peak_idx}")
    print(f"峰值幅度: {abs_val[peak_idx]}")

if __name__ == "__main__":
    plot_verification()
    