import numpy as np
import matplotlib.pyplot as plt

# ==========================================
# 配置参数 (与 HLS 保持一致)
# ==========================================
N_RANGE = 128
N_PULSE = 128
FS = 20000.0
B = 10000.0
ADC_BITS = 14

# --- 设定测试目标 ---
# 你可以在这里修改目标位置，看看 HLS 能不能算对
TARGET_RANGE_IDX = 50   # 目标在距离门 50
TARGET_DOPPLER_IDX = 32 # 目标在多普勒通道 32 (模拟移动目标)

def generate_full_chain_stimulus():
    # 1. 生成基础 LFM 信号
    T_pulse = N_RANGE / FS
    K = B / T_pulse
    t = np.arange(N_RANGE) / FS
    lfm_sig = np.exp(1j * np.pi * K * t**2)

    # 2. 生成系数文件 (频域共轭)
    ref_fft = np.fft.fft(lfm_sig, n=N_RANGE)
    coeffs = np.conj(ref_fft)
    coeffs_norm = coeffs / np.max(np.abs(coeffs)) # 归一化

    # 写入 radar_coeffs.h
    print(f"生成 radar_coeffs.h (Range={N_RANGE})...")
    with open("radar_coeffs.h", "w") as f:
        for i, val in enumerate(coeffs_norm):
            comma = "," if i < N_RANGE - 1 else ""
            f.write(f"{{{val.real:.6f}, {val.imag:.6f}}}{comma}\n")

    # 3. 生成含有 多普勒频移 的回波数据
    # -------------------------------------------------
    print(f"生成测试激励: Target @ Range {TARGET_RANGE_IDX}, Doppler {TARGET_DOPPLER_IDX}")
    
    # 基础回波 (距离延迟)
    base_echo = np.roll(lfm_sig, TARGET_RANGE_IDX)
    # 简单的噪声幅度
    noise_level = 0.05 

    # 打开文件准备写入
    with open("input_stimulus.dat", "w") as f:
        
        # 循环生成 N_PULSE 个脉冲
        for p in range(N_PULSE):
            # 计算多普勒相位旋转: exp(j * 2*pi * k * p / N)
            # k = target_doppler, p = pulse_index
            phase_shift = np.exp(1j * 2 * np.pi * TARGET_DOPPLER_IDX * p / N_PULSE)
            
            # 当前脉冲 = 基础回波 * 相位旋转
            pulse_sig = base_echo * phase_shift
            
            # 加噪声
            noise = (np.random.randn(N_RANGE) + 1j*np.random.randn(N_RANGE)) * noise_level
            rx_sig = pulse_sig + noise
            
            # 量化到 14位
            scale = 2**(ADC_BITS-1) - 1
            rx_fixed = np.round(rx_sig * scale)
            rx_fixed = np.clip(rx_fixed, -scale, scale)
            
            # 写入文件
            for i in range(N_RANGE):
                re = int(rx_fixed[i].real)
                im = int(rx_fixed[i].imag)
                f.write(f"{re} {im}\n")

    print("input_stimulus.dat 生成完毕。")

if __name__ == "__main__":
    generate_full_chain_stimulus()