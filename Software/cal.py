import math

def calculate_buck_boost_limits(L_uH, fc_Hz, R_load, margin=4):
    """
    计算Buck-Boost电源在指定穿越频率下的占空比和增益限制
    
    :param L_uH: 电感值 (微亨)
    :param fc_Hz: 目标穿越频率 (Hz)
    :param R_load: 负载电阻 (欧姆)
    :param margin: RHPZ需要是穿越频率的几倍 (通常取 3 到 4)
    """
    L = L_uH * 1e-6
    # 目标最小RHPZ频率
    f_rhpz_min = fc_Hz * margin
    
    # 设 K = f_rhpz_min * 2 * pi * L
    K = f_rhpz_min * 2 * math.pi * L
    
    # 由 f_rhpz = R*(1-D)^2 / (2*pi*L*D) 推导一元二次方程: R*D^2 - (2R+K)*D + R = 0
    a = R_load
    b = -(2 * R_load + K)
    c = R_load
    
    # 计算判别式
    desc = b**2 - 4 * a * c
    if desc < 0:
        return None, None
        
    # 求解二次方程 (取小于1的那个根)
    D_max = (-b - math.sqrt(desc)) / (2 * a)
    
    # 计算最大电压增益 (Buck-Boost 理想增益公式 Vout/Vin = D/(1-D))
    Gain_max = D_max / (1 - D_max)
    
    return D_max, Gain_max

# ================= 运行测试 =================
if __name__ == "__main__":
    L_val = 33        # 电感 33uH
    fc_target = 500  # 目标穿越频率 1kHz
    
    # 模拟不同的负载情况
    test_loads = [2, 4, 6, 10, 20] # 欧姆
    
    print(f"设定参数: L = {L_val}uH, 目标穿越频率 fc = {fc_target}Hz (要求 RHPZ >= {fc_target*4}Hz)\n")
    print(f"{'负载(Ω)':<10} | {'最大允许占空比(D_max)':<20} | {'最大电压升压比(Vout/Vin)':<20}")
    print("-" * 55)
    
    for R in test_loads:
        d_max, gain_max = calculate_buck_boost_limits(L_val, fc_target, R)
        if d_max:
            print(f"{R:<10} | {d_max*100:>15.2f}% | {gain_max:>18.2f} 倍")
        else:
            print(f"{R:<10} | {'无法实现':>15} | {'无法实现':>18}")