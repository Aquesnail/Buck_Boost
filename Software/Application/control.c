#include "control.h"
#include "stm32g431xx.h"
#include "tim.h"
#include "stm32g4xx_hal_tim.h"
#include "ui_data.h"
#include <stdint.h>
#include "adc.h"
#include <math.h> // 需要用到 M_PI，或者自己定义宏

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// 定义低通滤波器结构体
typedef struct {
    float out;      // 上一次的输出 Y[n-1]
    float alpha;    // 滤波系数
    uint8_t init;   // 初始化标志，防止系统启动时由于 out 为 0 导致巨大的阶跃
} FirstOrderLPF_t;

/**
 * @brief 初始化/更新滤波器的截止频率
 * @param lpf 滤波器实例
 * @param fc_hz 截止频率 (Hz)
 * @param Ts 采样周期 (s)
 */
void LPF_CalcAlpha(FirstOrderLPF_t *lpf, float fc_hz, float Ts) {
    // 根据公式计算 alpha
    lpf->alpha = (2.0f * M_PI * fc_hz * Ts) / (1.0f + 2.0f * M_PI * fc_hz * Ts);
}

/**
 * @brief 执行单次滤波
 * @param lpf 滤波器实例
 * @param input 最新采样值
 * @return 滤波后的值
 */
float LPF_Update(FirstOrderLPF_t *lpf, float input) {
    if (lpf->init == 0) {
        // 第一次运行，直接将当前值赋给 out，避免从 0 开始缓慢爬升
        lpf->out = input;
        lpf->init = 1;
    } else {
        // Y[n] = α * X[n] + (1-α) * Y[n-1]
        lpf->out = (lpf->alpha * input) + ((1.0f - lpf->alpha) * lpf->out);
    }
    return lpf->out;
}

// --- 配置参数 ---
#define ADC1_CH_NUM    2    // ADC1 有 2 个通道
#define ADC2_CH_NUM    4    // ADC2 有 4 个通道
#define BUF_DEPTH      4    // 每个通道缓存 4 组数据（用于平滑）

#define SET_PWM_DUTY_HIGH_RES(HANDLE, CH, VALUE)  __HAL_TIM_SET_COMPARE(HANDLE, CH, VALUE)


PowerMeas_t powerMeas = {0};
PowerState_t powerState = {0};
uint16_t adc_buffer1[ADC1_CH_NUM * BUF_DEPTH]; 
uint16_t adc_buffer2[ADC2_CH_NUM * BUF_DEPTH];

float adc_voltages[6]; // [0,1]来自ADC1, [2,3,4,5]来自ADC2

void SetBuckDuty(float duty){
    
}
void SetBoostDuty(float duty){

}

void Control_Init(void){
    // ADC1: 2通道 * 4深度 = 8 个 Halfword
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer1, ADC1_CH_NUM * BUF_DEPTH);
    powerState.target_v = 5.2f;
    powerState.target_i = 2.0f;
    powerState.pi_ki = 150.0f;
    powerState.pi_kp = 0.15f;
    // ADC2: 4通道 * 4深度 = 16 个 Halfword
    // 注意：你之前的 adc_buffer2[12] 长度不够 16，会导致内存溢出，已修正
    HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc_buffer2, ADC2_CH_NUM * BUF_DEPTH);
   
}
// HAL库标准的常规转换完成回调函数
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        // 处理 ADC1 的 2 个通道 (求平均值并转为电压)
        for (int ch = 0; ch < ADC1_CH_NUM; ch++) {
            uint32_t sum = 0;
            for (int i = 0; i < BUF_DEPTH; i++) {
                sum += adc_buffer1[i * ADC1_CH_NUM + ch];
            }
            // 65535.0f 是因为 12位ADC + 16倍过采样 (无位移)
            adc_voltages[ch] = ((float)sum / BUF_DEPTH) * 3.3f / 65535.0f * 2.0f;
        }
    }
    else if (hadc->Instance == ADC2) {
        // 处理 ADC2 的 4 个通道
        for (int ch = 0; ch < ADC2_CH_NUM; ch++) {
            uint32_t sum = 0;
            for (int i = 0; i < BUF_DEPTH; i++) {
                sum += adc_buffer2[i * ADC2_CH_NUM + ch];
            }
            // 存入索引 2, 3, 4, 5
            adc_voltages[ADC1_CH_NUM + ch] = ((float)sum / BUF_DEPTH) * 3.3f / 65535.0f * 2.0f;
        }
    }
}

/*
数组索引,来源,ADC 通道,原理图信号,物理意义
adc_voltages[0],ADC1 Rank 1,IN15,ADC_IL,电感电流 / 电流采样 L
adc_voltages[1],ADC1 Rank 2,IN12,ADC_I_OUT,输出电流
---,---,---,---,---
adc_voltages[2],ADC2 Rank 1,IN12,ADC_VOUT,输出电压
adc_voltages[3],ADC2 Rank 2,IN4,ADC_I_IN,输入电流
adc_voltages[4],ADC2 Rank 3,IN3,ADC_VIN,输入电压
adc_voltages[5],ADC2 Rank 4,IN13,TEMP,温度采样
*/
// (849 * 16) + 15 = 13599
#define PWM_RESOLUTION_MAX    13599.0f 
// 自举电容充电所需的最小关断时间比例 (10%)
#define BOOTSTRAP_MIN_RECHARGE_RATIO 0.10f


 #define SET_PWM_DUTY_HIGH_RES(HANDLE, CH, VALUE)  __HAL_TIM_SET_COMPARE(HANDLE, CH, VALUE)

/**
 * @brief 更新 Buck 占空比 (0.0 ~ 1.0)
 * @param duty: 目标占空比。0.9 代表上管导通 90%。
 */
void Update_Buck_Duty(float duty) {
    // 1. 安全限幅：因为自举限制，上管最大只能开到 90%
    if (duty > (1.0f - BOOTSTRAP_MIN_RECHARGE_RATIO)) {
        duty = 1.0f - BOOTSTRAP_MIN_RECHARGE_RATIO;
    }
    if (duty < 0.0f) duty = 0.0f;

    // 2. 映射计算：Buck 上管占空比 = 1 - (CCR1 / MAX)
    // 所以 CCR1 = MAX * (1 - duty)
    uint32_t ccr_value = (uint32_t)(PWM_RESOLUTION_MAX * (1.0f - duty));

    // 3. 写入寄存器
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, ccr_value);
}

/**
 * @brief 更新 Boost 占空比 (0.0 ~ 1.0)
 * @param duty: 目标占空比。0.1 代表下管导通 10%。
 */
void Update_Boost_Duty(float duty) {
    // 1. 安全限幅：为了给 Boost 上管自举充电，下管最大只能开到 90%
    if (duty > (1.0f - BOOTSTRAP_MIN_RECHARGE_RATIO)) {
        duty = 1.0f - BOOTSTRAP_MIN_RECHARGE_RATIO;
    }
    if (duty < 0.0f) duty = 0.0f;

    // 2. 映射计算：Boost 下管占空比 = CCR3 / MAX
    uint32_t ccr_value = (uint32_t)(PWM_RESOLUTION_MAX * duty);

    // 3. 写入寄存器
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, ccr_value);
}


static float buck_duty_int = 0.0f; 
const float Ts = 0.0001f; // 10kHz 采样周期
uint32_t tt=0;
static FirstOrderLPF_t vout_lpf = {0, 0, 0}; // 定义 Vout 的低通滤波器
static uint8_t ctrl_initialized = 0;

#define STARTUP_DELAY_MS    20.0f
#define STARTUP_DELAY_TICKS (uint32_t)(STARTUP_DELAY_MS / (Ts * 1000.0f)) // 计算得 200
// --- 静态变量 ---
static uint32_t startup_counter = 0;
static uint8_t is_power_ready = 0;

void Control_Tick_Hook(void){
    // 1. 初始化控制参数（仅执行一次）
    if (!ctrl_initialized) {
        // Ts = 0.0001f (10kHz)
        // fc = 1000.0f Hz (建议设置为你期望穿越频率的 5~10 倍)
        LPF_CalcAlpha(&vout_lpf, 1500.0f, Ts); 
        ctrl_initialized = 1;
    }
    
    // powerState.pi_ki = 1.0f;
    // powerState.pi_kp = 1.0f;
    // powerState.output_en = 1;

    //powerMeas.vout = adc_voltages[2]*12.0f;
    powerMeas.iin = (adc_voltages[3]-1.65f)*200.0f/100.0f;
    powerMeas.vin = adc_voltages[4]*12.0f;
    powerMeas.temp = adc_voltages[5];

    powerMeas.inductor_i = adc_voltages[0]*20.0f/100.0f;
    powerMeas.iout = -(adc_voltages[1]-1.608)*200.0f/100.0f;

    float raw_vout = adc_voltages[2] * 12.0f;
    powerMeas.vout = LPF_Update(&vout_lpf, raw_vout);

    if (startup_counter < STARTUP_DELAY_TICKS) {
        startup_counter++;
        
        // --- 在等待期执行的操作 ---
        Update_Buck_Duty(0.0f);   // 强制关断 Buck 上管
        Update_Boost_Duty(0.1f);  // 仅维持最低占空比给自举电容充电
        
        buck_duty_int = 0.0f;     // 重要：在等待期不断重置积分项，防止积分过热
        is_power_ready = 0;
        return;                   // 跳过本次 PI 计算
    } else {
        is_power_ready = 1;       // 过了 20ms，标记为就绪
    }

    float target_v = powerState.target_v;
    float current_v = powerMeas.vout;
    float err = target_v - current_v;

    // --- 调整后的 PI 计算 ---
    float Kp = 0.01f;  // 降低 Kp，避开谐振点震荡
    float Ki = 15.0f; // Ki 对应连续域的增益，乘上 Ts 后才是每步步进

    // 积分项更新 (显式包含 Ts)
    buck_duty_int += Ki * err * Ts;

    // 抗饱和限幅
    if (buck_duty_int > 0.85f) buck_duty_int = 0.85f;
    if (buck_duty_int < 0.0f)  buck_duty_int = 0.0f;

    float duty_out = (Kp * err) + buck_duty_int;

    // 最终输出限幅
    if (duty_out > 0.9f) duty_out = 0.9f;
    if (duty_out < 0.0f) duty_out = 0.0f;

    if(is_power_ready){
        Update_Buck_Duty(duty_out);
        Update_Boost_Duty(0.1f); // 维持自举充电
    }
}




void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC2) {
        // 如果运行到这里，说明 ADC2 溢出或者配置冲突了
        // 常见的错误码是 HAL_ADC_ERROR_DMA
        uint32_t err = HAL_ADC_GetError(hadc);
    }
}