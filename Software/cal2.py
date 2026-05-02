import math

def evaluate_buck_boost_system(L_uH, Cout_uF, Vout_nom, fc_Hz, R_load, delta_I, ff_delay_us=100, margin=4):
    """
    全面评估Buck-Boost电源在特定参数下的频域边界与时域阶跃响应
    
    :param L_uH: 电感 (微亨)
    :param Cout_uF: 输出电容 (微法)
    :param Vout_nom: 标称输出电压 (V)，用于计算跌落百分比
    :param fc_Hz: 目标穿越频率 (Hz)
    :param R_load: 初始负载电阻 (欧姆)
    :param delta_I: 负载阶跃电流突变量 (A)
    :param ff_delay_us: 前馈控制的采样/计算延迟时间 (微秒)，默认100us (对应10kHz)
    :param margin: RHPZ安全裕度 (倍数)
    """
    L = L_uH * 1e-6
    C = Cout_uF * 1e-6
    T_delay = ff_delay_us * 1e-6
    
    # ---------------- 1. 频域/占空比边界计算 ----------------
    f_rhpz_min = fc_Hz * margin
    K = f_rhpz_min * 2 * math.pi * L
    
    a = R_load
    b = -(2 * R_load + K)
    c = R_load
    
    desc = b**2 - 4 * a * c
    if desc < 0:
        D_max, Gain_max = None, None
    else:
        D_max = (-b - math.sqrt(desc)) / (2 * a)
        Gain_max = D_max / (1 - D_max)
        
    # ---------------- 2. 时域/动态响应计算 ----------------
    # PID 环路物理响应时间 (约等于 1 / 2*pi*fc)
    t_response_pid = 1 / (2 * math.pi * fc_Hz)
    
    # 纯 PID 控制下的电压跌落 (电容放电时间 = 环路响应时间)
    v_dip_pid = (delta_I * t_response_pid) / C
    v_dip_pid_percent = (v_dip_pid / Vout_nom) * 100
    
    # 加入前馈控制后的电压跌落 (电容放电时间 = 前馈采样延迟)
    v_dip_ff = (delta_I * T_delay) / C
    v_dip_ff_percent = (v_dip_ff / Vout_nom) * 100
    
    return {
        "D_max": D_max,
        "Gain_max": Gain_max,
        "t_res_pid_ms": t_response_pid * 1000,
        "dip_pid_v": v_dip_pid,
        "dip_pid_pct": v_dip_pid_percent,
        "dip_ff_v": v_dip_ff,
        "dip_ff_pct": v_dip_ff_percent
    }

# ================= 测试运行 =================
if __name__ == "__main__":
    # --- 你的硬件参数 ---
    L_val = 33            # 33 uH
    C_val = 940           # 470uF * 2 = 940 uF
    Vout = 12.0           # 假设输出 12V
    
    # --- 环路与负载参数 ---
    fc_target = 1000       # 由于 MCU 算力和占空比限制，保守设定为 100Hz
    R_init = 2.0          # 初始负载 6Ω (约2A)
    step_I = 4.0          # 负载瞬间再跳变增加 2A
    ff_delay = 100        # 前馈执行周期 100us (即 10kHz)
    
    print("="*60)
    print(f"Buck-Boost 动态响应与稳定性综合评估报告")
    print("="*60)
    print(f"[硬件配置] L = {L_val}uH, Cout = {C_val}uF, 标称Vout = {Vout}V")
    print(f"[环路设定] 目标带宽 fc = {fc_target}Hz, 前馈执行频率 = {1000000/ff_delay:.0f}Hz")
    print(f"[阶跃工况] 初始负载 = {R_init}Ω, 突增电流 = {step_I}A")
    print("-" * 60)
    
    res = evaluate_buck_boost_system(L_val, C_val, Vout, fc_target, R_init, step_I, ff_delay)
    
    if res["D_max"] is None:
        print("❌ [严重警告] 在该负载下，无法将RHPZ推到安全范围外，系统必震荡！")
        print("   -> 建议：降低穿越频率，或减小电感感值。")
    else:
        print(f"【频域稳定性边界】")
        print(f"  • 为保证 {fc_target}Hz 稳定，最高允许占空比: {res['D_max']*100:.2f}%")
        print(f"  • 对应的最大升压比 (Vout/Vin): {res['Gain_max']:.2f} 倍")
        print(f"\n【时域动态响应评估 (阶跃 {step_I}A)】")
        print(f"  • PID 理论响应时间: {res['t_res_pid_ms']:.2f} ms")
        print(f"\n  [方案 A] 纯 PID 单电压环控制 (无前馈)")
        print(f"    - 理论电压跌落: {res['dip_pid_v']:.2f} V")
        print(f"    - 跌落幅度: {res['dip_pid_pct']:.2f} % " + ("(❌ 严重超标)" if res['dip_pid_pct'] > 5 else "(✅ 及格)"))
        
        print(f"\n  [方案 B] PID + {1000000/ff_delay:.0f}Hz 前馈补偿 (你的方案)")
        print(f"    - 理论电压跌落: {res['dip_ff_v']:.3f} V")
        print(f"    - 跌落幅度: {res['dip_ff_pct']:.2f} % " + ("(❌ 不及格)" if res['dip_ff_pct'] > 5 else "(✅ 极佳)"))
    print("="*60)