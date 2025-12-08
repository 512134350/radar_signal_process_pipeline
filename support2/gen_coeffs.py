import numpy as np

N_POINTS = 1024
SAMPLE_RATE = 100e6
PULSE_WIDTH = 10.24e-6  # 覆盖整个采样窗口
BANDWIDTH = 20e6

def generate_coeffs():
    Ts = 1 / SAMPLE_RATE
    t = np.arange(0, N_POINTS) * Ts  # 正时间轴
    k = BANDWIDTH / PULSE_WIDTH

    # Chirp
    chirp = np.exp(1j * np.pi * k * np.square(t))

    # ---- 匹配滤波核心：时间反转 + 共轭 ----
    h = np.conj(chirp[::-1])

    # 频域
    H = np.fft.fft(h)

    # 按 ap_fixed<16,1> 归一化
    scale = 0.9 / np.max(np.abs(H))
    H = H * scale

    with open("radar_coeffs.h", "w") as f:
        for i in range(N_POINTS):
            f.write(f"{{{H[i].real:.6f}, {H[i].imag:.6f}}}")
            if i != N_POINTS - 1:
                f.write(",\n")

    print("Done! Correct radar_coeffs.h generated.")

if __name__ == "__main__":
    generate_coeffs()
