import numpy as np
import matplotlib.pyplot as plt

# ==========================================
# 1. 项目参数配置 (对应题目要求)
# ==========================================
N_RANGE = 128          # 距离门数 (题目约束)
N_PULSE = 128          # 脉冲数 (用于后续多普勒，这里主要测脉压)
FS = 20000.0           # 采样率
B = 10000.0            # 带宽 10kHz
ADC_BITS = 14          # 输入 14位 ADC

# ==========================================
# 2. 生成 LFM 参考信号与系数
# ==========================================
def generate_files():
    # --- A. 生成 LFM 信号 ---
    T_pulse = N_RANGE / FS
    K = B / T_pulse
    t = np.arange(N_RANGE) / FS
    
    # 理想 LFM 信号
    lfm_sig = np.exp(1j * np.pi * K * t**2)
    
    # --- B. 生成匹配滤波器系数 (频域共轭) ---
    ref_fft = np.fft.fft(lfm_sig, n=N_RANGE)
    coeffs = np.conj(ref_fft)
    
    # 归一化系数 (防止溢出)
    max_abs_coeff = np.max(np.abs(coeffs))
    coeffs_norm = coeffs / max_abs_coeff
    
    # 写入 radar_coeffs.h
    print("生成 radar_coeffs.h ...")
    with open("radar_coeffs.h", "w") as f:
        for i, val in enumerate(coeffs_norm):
            comma = "," if i < N_RANGE - 1 else ""
            f.write(f"{{{val.real:.6f}, {val.imag:.6f}}}{comma}\n")

    # --- C. 生成测试激励 (模拟回波) ---
    # 模拟一个目标在 index = 50 的位置
    target_idx = 50
    echo_sig = np.roll(lfm_sig, target_idx) 
    
    # 添加高斯白噪声 (SNR控制)
    noise = (np.random.randn(N_RANGE) + 1j*np.random.randn(N_RANGE)) * 0.1
    rx_sig = echo_sig + noise
    
    # 量化到 14位 (模拟 ADC)
    # 范围映射到 [-1, 1) -> [-8192, 8191]
    scale_adc = 2**(ADC_BITS-1) - 1
    rx_fixed = np.round(rx_sig * scale_adc)
    rx_fixed = np.clip(rx_fixed, -scale_adc, scale_adc)
    
    # 写入 input_stimulus.dat (实部 虚部)
    print("生成 input_stimulus.dat ...")
    with open("input_stimulus.dat", "w") as f:
        # 生成 N_PULSE 个脉冲的数据 (这里简单复制，模拟静止目标，主要测脉压)
        for p in range(N_PULSE):
            for i in range(N_RANGE):
                re = int(rx_fixed[i].real)
                im = int(rx_fixed[i].imag)
                f.write(f"{re} {im}\n")
    
    # --- D. 计算黄金参考 (Golden Reference) ---
    # 频域脉冲压缩: IFFT( FFT(Signal) * Coeffs )
    rx_fft = np.fft.fft(rx_sig, n=N_RANGE)
    pc_res_freq = rx_fft * coeffs_norm # 使用归一化后的系数
    pc_res_time = np.fft.ifft(pc_res_freq)
    
    # 保存黄金参考供对比绘图 (保存模值即可)
    np.savetxt("golden_abs.dat", np.abs(pc_res_time))
    
    print("脚本执行完毕。请运行 HLS C Simulation，然后运行 plot_result.py")

if __name__ == "__main__":
    generate_files()