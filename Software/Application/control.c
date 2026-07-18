#include "control.h"
#include "stm32g431xx.h"
#include "tim.h"
#include "stm32g4xx_hal_tim.h"
#include "ui_data.h"
#include <stdint.h>
#include "adc.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif
#define TWO_PI (2.0f * M_PI)
#define ADC_AVG10_SCALE (3.3f / 4095.0f / 10.0f)

#define CTRL_FC_HZ  500.0f   // 控制环 LPF 截止频率
#define DISP_AVG_N  1000     // 显示暴力平均样本数

// --- 低通滤波器 ---
typedef struct {
    float out;
    float alpha;
    uint8_t init;
} FirstOrderLPF_t;

void LPF_CalcAlpha(FirstOrderLPF_t *lpf, float fc_hz, float Ts) {
    float omega_T = TWO_PI * fc_hz * Ts;
    lpf->alpha = omega_T / (1.0f + omega_T);
}

float LPF_Update(FirstOrderLPF_t *lpf, float input) {
    if (lpf->init == 0) {
        lpf->out = input;
        lpf->init = 1;
    } else {
        lpf->out += lpf->alpha * (input - lpf->out);
    }
    return lpf->out;
}

// --- PI 参数结构体 ---
typedef struct {
    float kp;
    float ki;
    float integral;
} PI_Controller_t;

// --- 4开关统一硬件配置常量 ---
#define CC_DUTY_MAX     13599
#define REG_BUCK_MAX    12239   // Buck 最大 90% 占空比限制
#define REG_BOOST_MIN   1360    // Boost 最小 10% 占空比限制（留给自举电容充电）
#define REG_UNIFIED_MAX 23118   // 统一控制量的最大上限: 12239 + (12239 - 1360)

// --- 环路控制器变量 ---
// --- 环路控制器变量 ---
static PI_Controller_t v_pi = {5.3f, 2000.0f, 0.0f}; // 统一后的电压外环 PI 参数

// 【修改】内环直接使用标准浮点 PI 结构体
typedef struct {
    float kp;
    float ki;
    float integral; 
} Float_PI_t;

// 内环浮点 PI，在归一化域运算（误差已通过 ERR_TO_CCR_GAIN 映射到 CCR 域）
// KP=0.5: 满量程误差(6.4A)时 P 项贡献 ~50% 占空比
// KI=0.1: 满量程误差时积分约 10 秒爬满，小误差时更慢，避免过冲
static Float_PI_t i_float_ctrl = {0.217f, 1408.0f, 0.0f}; 

// 【修改】内外环隔离调试：直接定义内环浮点目标电流（安培）
// 你可以在运行调试时，通过仿真器直接修改这个值（例如改为 1.5f 代表恒流 1.5A）
static volatile float target_il_amps = 0.4f;

// --- 全局变量 ---
PowerMeas_t powerMeas = {0};
PowerMeas_t powerMeasDisp = {0};
PowerState_t powerState = {0};

#define ADC1_CH_NUM 2
#define ADC2_CH_NUM 4
#define BUF_DEPTH   4

volatile uint16_t adc_buffer1[ADC1_CH_NUM];
volatile uint16_t adc_buffer2[ADC2_CH_NUM * BUF_DEPTH];
float adc_voltages[6];

static const float Ts = 0.0001f; // 10kHz 外环采样周期
static FirstOrderLPF_t vout_lpf = {0, 0, 0};
static FirstOrderLPF_t vin_lpf = {0, 0, 0};
static FirstOrderLPF_t iin_lpf = {0, 0, 0};
static FirstOrderLPF_t il_lpf   = {0, 0, 0};
static FirstOrderLPF_t iout_lpf = {0, 0, 0};
static FirstOrderLPF_t il_rt_lpf = {0, 0, 0};

// 仅用于遥测/显示降采样的累加器
static uint32_t telemetry_csum0, telemetry_csum1;
static uint8_t telemetry_cnt;
static float measured_il, measured_iout;

static float disp_sum_vout, disp_sum_vin, disp_sum_iin;
static float disp_sum_iout, disp_sum_il, disp_sum_temp;
static uint16_t disp_avg_cnt;
static uint8_t ctrl_initialized = 0;
static uint32_t startup_counter = 0;
static uint8_t is_power_ready = 0;
static uint8_t uvlo_fault = 0;
static uint8_t open_loop_enable = 0;
static volatile int32_t open_loop_ccr = 0;

/* 电压目标斜率限制: 1V/5ms = 200V/s */
#define SLEW_RATE_V_PER_S      200.0f
static float   target_v_slewed    = 0.0f;
static uint8_t slew_active_prev   = 0;

/* CC 注入模式: 电流外环 PI + 斜率限制 */
#define CC_SLEW_RATE_A_PER_S    100.0f   /* 0.5A/5ms = 100A/s */
static PI_Controller_t cc_pi           = {0.5f, 500.0f, 0.0f};
static float           target_i_slewed  = 0.0f;
static uint8_t         cc_mode_prev     = 0;

#define STARTUP_DELAY_MS    20.0f
#define STARTUP_DELAY_TICKS (uint32_t)(STARTUP_DELAY_MS / (Ts * 1000.0f))

void Control_Init(void) {
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer1, ADC1_CH_NUM);
    HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc_buffer2, ADC2_CH_NUM * BUF_DEPTH);
    __HAL_DMA_DISABLE_IT(hadc1.DMA_Handle, DMA_IT_HT);
    __HAL_DMA_DISABLE_IT(hadc2.DMA_Handle, DMA_IT_HT);
    powerState.target_v = 1.5f;
    powerState.target_i = 3.0f;
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, 848);
}

// --- 电流内环归一化：将 amp 误差线性映射到统一 CCR 域 ---
// 满量程 6.4A → REG_UNIFIED_MAX (23118)，PI 在归一化域内运算
#define CURRENT_FS_AMPS   6.4f
#define ERR_TO_CCR_GAIN   ((float)REG_UNIFIED_MAX / CURRENT_FS_AMPS)  // ≈ 3612

// --- 4开关三段式过渡区定义 ---
// 混合过渡区宽度 (筹码总宽度的约 5% ~ 8%)
#define OVERLAP_WIDTH     1000

// 三段式区域边界
#define ZONE1_BUCK_END    (REG_BUCK_MAX - OVERLAP_WIDTH)   // 纯Buck结束点：11239
#define ZONE2_BOOST_START (REG_BUCK_MAX)                   // 纯Boost开始点：12239

/**
  * @brief  工业级三段式四开关占空比分发器 (带 Buck-Boost 混合过渡区)
  * @note   运行在 100kHz 内环末尾，纯整数运算，无分支损耗
  */
static inline void Drive_4Switch_Bridge(int32_t ccr_unified) {
    uint32_t buck_ccr;
    uint32_t boost_ccr;

    if (ccr_unified < ZONE1_BUCK_END) {
        // ==========================================
        // 区域 1：纯 Buck 调制区
        // ==========================================
        buck_ccr = CC_DUTY_MAX - ccr_unified; 
        boost_ccr = REG_BOOST_MIN; // Boost 下管稳锁 10%
    } 
    else if (ccr_unified <= ZONE2_BOOST_START) {
        // ==========================================
        // 区域 2：混合过渡区 (Buck-Boost 同时切换)
        // ==========================================
        // 在这 1000 个筹码的宽度内：
        // Buck 占空比从 90% 平滑限制到 85% 左右，Boost 占空比从 10% 平滑爬升到 15%
        int32_t local_offset = ccr_unified - ZONE1_BUCK_END; // 0 ~ OVERLAP_WIDTH

        // Buck 臂在过渡区内微微让出一点占空比斜率
        uint32_t buck_duty_mapped = REG_BUCK_MAX - (local_offset >> 1); 
        buck_ccr = CC_DUTY_MAX - buck_duty_mapped;

        // Boost 臂同步开始平滑注入占空比
        boost_ccr = REG_BOOST_MIN + (local_offset >> 1);
    } 
    else {
        // ==========================================
        // 区域 3：纯 Boost 调制区
        // ==========================================
        buck_ccr = CC_DUTY_MAX - REG_BUCK_MAX; // Buck 稳锁 90%
        boost_ccr = REG_BOOST_MIN + (ccr_unified - ZONE2_BOOST_START);
    }

    // 硬件底层单周期直写
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, buck_ccr);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, boost_ccr);
}

void Power_Set_Safe_PWM(void) {
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, CC_DUTY_MAX); // Buck 占空比 0%
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 0);           // Boost 占空比 0%
}

/**
  * @brief  进入开环测试模式，固定 ccr_unified 直接驱动四开关桥臂
  * @param  ccr_unified: 统一控制量，范围 0 ~ REG_UNIFIED_MAX (23118)
  *         0     = Buck 100% / Boost   0%
  *         11239 = Buck  90% / Boost  10% (纯Buck边界)
  *         12239 = Buck  90% / Boost  15% (纯Boost边界)
  *         23118 = Buck  90% / Boost 100%
  */
void Power_Set_OpenLoop_Duty(int32_t ccr_unified) {
    // 清零 PI 积分，防止切回闭环时 windup
    i_float_ctrl.integral = 0.0f;
    v_pi.integral = 0.0f;

    // 限幅
    if (ccr_unified > REG_UNIFIED_MAX) ccr_unified = REG_UNIFIED_MAX;
    if (ccr_unified < 0)              ccr_unified = 0;

    open_loop_ccr = ccr_unified;
    open_loop_enable = 1;
}

/**
  * @brief  退出开环测试模式，恢复正常闭环控制
  * @note   退出后需等待软启动延时 (STARTUP_DELAY_MS) 才会重新使能功率输出
  */
void Power_Exit_OpenLoop(void) {
    open_loop_enable = 0;
    Power_Set_Safe_PWM();
    // 触发软启动流程重置
    startup_counter = 0;
    is_power_ready = 0;
}

#define Vout_K  12.0f
#define Vout_B  0.0965f
#define Vin_K 12.0f
#define Vin_B 0.0465f
#define Iout_K (-3.8488f)
#define Iout_B 6.0658f
#define Iin_K (-3.92428f)
#define Iin_B 6.5696f
#define IL_K (-4.0f)
#define IL_B 6.6f
#define ADC_TO_IL_K     ((3.3f / 4095.0f) * IL_K) // 合并后的斜率
#define ADC_TO_IL_B     (IL_B)                    // 截距
/**
  * @brief  电流内环 - 运行在 100kHz 中断 (由 ADC 触发)
  * @note   误差归一化到 CCR 域后再进 PI，KP/KI 量纲统一、参数直观
  */
void control_current(void) {
    // 1. 读取单次 12 位电感电流 ADC 原始值
 //   HAL_GPIO_WritePin(GPIOB,GPIO_PIN_3,GPIO_PIN_SET);
    uint16_t raw_il_adc = adc_buffer1[0];

    float raw_il_amps = (float)raw_il_adc * ADC_TO_IL_K + ADC_TO_IL_B;
    // 2. FMA 单周期换算为物理电流 (安培)
    float current_il_amps = LPF_Update(&il_rt_lpf, raw_il_amps);

    // 3. 遥测累加 (供外环 LPF 和显示降采样使用)
    telemetry_csum0 += raw_il_adc;
    telemetry_csum1 += adc_buffer1[1];
    telemetry_cnt++;

    // 开环测试模式：跳过保护和PI，直接灌固定占空比
    if (open_loop_enable) {
        Drive_4Switch_Bridge(open_loop_ccr);
        return;
    }

    // 用户关闭输出
    if (!powerState.output_en) {
        i_float_ctrl.integral = 0.0f;
        Power_Set_Safe_PWM();
        return;
    }

    if (!is_power_ready || uvlo_fault) {
        i_float_ctrl.integral = 0.0f;
        Power_Set_Safe_PWM();
        return;
    }

    // 4. 物理误差 (安培) → 归一化到 CCR 计数域
    //    err_i > 0 → 电流不足 → PI 增大占空比
    float err_i = target_il_amps - current_il_amps;
    float err_norm = err_i * ERR_TO_CCR_GAIN;

    // 5. 浮点 PI 核心 (Ts = 10μs, 归一化域)
    i_float_ctrl.integral += i_float_ctrl.ki * err_norm * 0.00001f;

    // 积分项限幅 (CCR 域 0 ~ REG_UNIFIED_MAX)
    if (i_float_ctrl.integral > REG_UNIFIED_MAX) i_float_ctrl.integral = (float)REG_UNIFIED_MAX;
    if (i_float_ctrl.integral < 0.0f)            i_float_ctrl.integral = 0.0f;

    // 总输出 = P项 + I项 (已在 CCR 域)
    float total_out = (i_float_ctrl.kp * err_norm) + i_float_ctrl.integral;

    // 统一控制量限幅
    if (total_out > REG_UNIFIED_MAX) total_out = (float)REG_UNIFIED_MAX;
    if (total_out < 0.0f)            total_out = 0.0f;

    // 6. 分发到四开关 PWM
    Drive_4Switch_Bridge((int32_t)total_out);
//    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_3,GPIO_PIN_RESET);
}

void Control_Tick_Hook(void);
void control_voltage(void) {
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_3,GPIO_PIN_SET);
    // ADC2 数据更新
    adc_voltages[2] = (float)adc_buffer2[0] / (BUF_DEPTH) * (3.3f / 4095.0f);
    adc_voltages[3] = (float)adc_buffer2[1] / (BUF_DEPTH) * (3.3f / 4095.0f);
    adc_voltages[4] = (float)adc_buffer2[2] / (BUF_DEPTH) * (3.3f / 4095.0f);
    adc_voltages[5] = (float)adc_buffer2[3] / (BUF_DEPTH) * (3.3f / 4095.0f);

    Control_Tick_Hook();
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_3,GPIO_PIN_RESET);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC2) {
        (void)HAL_ADC_GetError(hadc);
    }
}

/**
  * @brief  电压外环 - 运行在 10kHz (主控制 Tick)
  */
void Control_Tick_Hook(void) {
    // 1. 初始化
    if (!ctrl_initialized) {
        LPF_CalcAlpha(&vout_lpf, CTRL_FC_HZ, Ts);
        LPF_CalcAlpha(&vin_lpf,  CTRL_FC_HZ, Ts);
        LPF_CalcAlpha(&iin_lpf,  CTRL_FC_HZ, Ts);
        LPF_CalcAlpha(&il_lpf,   2000.0f, Ts);
        LPF_CalcAlpha(&iout_lpf, CTRL_FC_HZ, Ts);
        LPF_CalcAlpha(&il_rt_lpf,30000.0f, 0.00001f); // 电流快速路径更高的截止频率
        ctrl_initialized = 1;
    }



    // 2. 遥测降采样计算 (原电流环移过来的 10 次后台平均，不阻碍 100kHz 实时控制)
    if (telemetry_cnt >= 10) {
        float avg_il_raw   = (float)telemetry_csum0 / telemetry_cnt;
        float avg_iout_raw = (float)telemetry_csum1 / telemetry_cnt;
        measured_il   = LPF_Update(&il_lpf,   avg_il_raw * (3.3f / 4095.0f)) * IL_K + IL_B;
        measured_iout = LPF_Update(&iout_lpf, avg_iout_raw * (3.3f / 4095.0f)) * Iout_K + Iout_B;
        
        telemetry_csum0 = 0;
        telemetry_csum1 = 0;
        telemetry_cnt = 0;
    }

    // 3. 换算物理量到 powerMeas
    powerMeas.temp = adc_voltages[5];
    powerMeas.inductor_i = measured_il;
    powerMeas.iout = measured_iout;

    float raw_vout = adc_voltages[2] * Vout_K + Vout_B;
    float raw_vin  = adc_voltages[4] * Vin_K + Vin_B;
    float raw_iin  = adc_voltages[3] * Iin_K + Iin_B;
    powerMeas.vout = LPF_Update(&vout_lpf, raw_vout);
    powerMeas.vin  = LPF_Update(&vin_lpf, raw_vin);
    powerMeas.iin  = LPF_Update(&iin_lpf, raw_iin);

    // [保留你原汁原味的暴力显示平均数据逻辑]
    disp_sum_vout += raw_vout;
    disp_sum_vin  += raw_vin;
    disp_sum_iin  += raw_iin;
    disp_sum_il   += measured_il;
    disp_sum_iout += measured_iout;
    disp_sum_temp += adc_voltages[5];

    if (++disp_avg_cnt >= DISP_AVG_N) {
        powerMeasDisp.vout       = disp_sum_vout / DISP_AVG_N;
        powerMeasDisp.vin        = disp_sum_vin  / DISP_AVG_N;
        powerMeasDisp.iin        = disp_sum_iin  / DISP_AVG_N;
        powerMeasDisp.iout       = disp_sum_iout / DISP_AVG_N;
        powerMeasDisp.inductor_i = disp_sum_il   / DISP_AVG_N;
        powerMeasDisp.temp       = disp_sum_temp / DISP_AVG_N;
        disp_sum_vout = 0.0f; disp_sum_vin = 0.0f; disp_sum_iin = 0.0f;
        disp_sum_iout = 0.0f; disp_sum_il = 0.0f; disp_sum_temp = 0.0f;
        disp_avg_cnt  = 0;
    }

    // 4. UVLO 欠压保护
    if (!uvlo_fault) {
        if (powerMeas.vin < 5.0f) uvlo_fault = 1;
    } else {
        if (powerMeas.vin > 6.0f) {
            uvlo_fault = 0;
            startup_counter = 0;
            is_power_ready = 0;
            v_pi.integral = 0.0f;
        }
    }

    if (uvlo_fault) {
        Power_Set_Safe_PWM();
        return;
    }

    // 5. 软启动与预准备
    if (startup_counter < STARTUP_DELAY_TICKS) {
        startup_counter++;
        Power_Set_Safe_PWM();
        v_pi.integral = 0.0f;
        is_power_ready = 0;
        return;
    } else {
        is_power_ready = 1;
    }

    // ==========================================
    // 6. 统一外环：CV 恒压 / CC 恒流注入（对称架构）
    // ==========================================

    // 6a. 模式切换检测：清零目标 PI，从当前实测值开始爬升
    if (powerState.cc_mode != cc_mode_prev) {
        if (powerState.cc_mode) {
            /* CV → CC：清零 CC PI，从当前 Iout 开始爬升 */
            cc_pi.integral   = 0.0f;
            target_i_slewed  = powerMeas.iout;
            slew_active_prev = 0;   /* 标记 CV slew 需要重新初始化 */
        } else {
            /* CC → CV：清零 CV PI，从当前 Vout 开始爬升 */
            v_pi.integral    = 0.0f;
            target_v_slewed  = powerMeas.vout;
        }
        cc_mode_prev = powerState.cc_mode;
    }

    float target_i_amps;

    if (powerState.cc_mode) {
        // ==========================================
        // CC 注入模式：电流外环调节 Iout → target_i
        //               target_v 作为合规电压上限
        // ==========================================

        /* CC 电流目标斜率限制 (0.5A/5ms = 100A/s) */
        {
            uint8_t cc_active = (is_power_ready && !uvlo_fault && powerState.output_en);

            if (!cc_active) {
                target_i_slewed = powerMeas.iout;
            } else {
                float step_max = CC_SLEW_RATE_A_PER_S * Ts;  /* 100 * 0.0001 = 0.01A/tick */
                float diff = powerState.target_i - target_i_slewed;
                if (diff > step_max) {
                    target_i_slewed += step_max;
                } else if (diff < -step_max) {
                    target_i_slewed -= step_max;
                } else {
                    target_i_slewed = powerState.target_i;
                }
            }
        }

        /* CC 电流 PI 外环 */
        float err_i = target_i_slewed - powerMeas.iout;
        cc_pi.integral += cc_pi.ki * err_i * Ts;
        if (cc_pi.integral > 6.5f)  cc_pi.integral = 6.5f;
        if (cc_pi.integral < -0.5f) cc_pi.integral = -0.5f;

        target_i_amps = (cc_pi.kp * err_i) + cc_pi.integral;

        /* 电压合规上限：Vout 超过 target_v 时压低电流 + 反饱和 */
        float err_vlim = powerMeas.vout - powerState.target_v;
        if (err_vlim > 0.0f) {
            target_i_amps -= 2.0f * err_vlim;   /* 2A/V 增益，足够压制 PI */
            /* 反饱和：电压限制激活时，钳位 PI 积分，防止对抗 */
            if (target_i_amps < 0.0f) {
                target_i_amps    = 0.0f;
                cc_pi.integral   = 0.0f;
            } else if (cc_pi.integral > target_i_amps) {
                cc_pi.integral   = target_i_amps;
            }
        }

    } else {
        // ==========================================
        // CV 恒压模式：电压外环调节 Vout → target_v
        //               target_i 作为过流限制
        // ==========================================

        /* 电压目标斜率限制 (1V/5ms = 200V/s) */
        {
            uint8_t slew_active = (is_power_ready && !uvlo_fault && powerState.output_en);

            if (!slew_active) {
                target_v_slewed = powerMeas.vout;
            } else if (!slew_active_prev) {
                target_v_slewed = powerMeas.vout;
            } else {
                float step_max = SLEW_RATE_V_PER_S * Ts;
                float diff = powerState.target_v - target_v_slewed;
                if (diff > step_max) {
                    target_v_slewed += step_max;
                } else if (diff < -step_max) {
                    target_v_slewed -= step_max;
                } else {
                    target_v_slewed = powerState.target_v;
                }
            }
            slew_active_prev = slew_active;
        }

        float err_v = target_v_slewed - powerMeas.vout;
        v_pi.integral += v_pi.ki * err_v * Ts;
        if (v_pi.integral > 6.5f)  v_pi.integral = 6.5f;
        if (v_pi.integral < -0.5f) v_pi.integral = -0.5f;

        target_i_amps = (v_pi.kp * err_v) + v_pi.integral;

        /* 恒流限制：Iout 超过 target_i 时压低电流 */
        float err_cc = powerMeas.iout - powerState.target_i;
        if (err_cc > 0.0f) {
            target_i_amps -= 0.5f * err_cc;
        }
    }

    // 物理电流总限幅安全护栏 (0A ~ 8A)
    if (target_i_amps > 8.0f) target_i_amps = 8.0f;
    if (target_i_amps < 0.0f) target_i_amps = 0.0f;

    // ==========================================
    // 7. 【关键多环无缝对接】：物理电流 (A) -> 12位单次 ADC 目标值
    // ==========================================
    // 根据你传感器的反向公式反推: Amps = V_adc * IL_K + IL_B
    // V_adc = (Amps - IL_B) / IL_K
    // 代入 IL_K = -4.0f, IL_B = 6.6f  =>  V_adc = (6.6f - Amps) * 0.25f
    float target_v_adc = (6.6f - target_i_amps) * 0.25f;
    
    // 将 0~3.3V 映射到 12位单次 ADC 整数范围 (0~4095)
    int32_t adc_target = (int32_t)(target_v_adc * (4095.0f / 3.3f));
    
    // 安全边界截断
    if (adc_target > 4095) adc_target = 4095;
    if (adc_target < 0)    adc_target = 0;

    // 电压外环 → 电流内环目标值 (物理安培)
    target_il_amps = target_i_amps;
}