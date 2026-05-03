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

// --- 低通滤波器 ---
typedef struct {
    float out;
    float alpha;
    uint8_t init;
} FirstOrderLPF_t;

void LPF_CalcAlpha(FirstOrderLPF_t *lpf, float fc_hz, float Ts) {
    lpf->alpha = (2.0f * M_PI * fc_hz * Ts) / (1.0f + 2.0f * M_PI * fc_hz * Ts);
}

float LPF_Update(FirstOrderLPF_t *lpf, float input) {
    if (lpf->init == 0) {
        lpf->out = input;
        lpf->init = 1;
    } else {
        lpf->out = (lpf->alpha * input) + ((1.0f - lpf->alpha) * lpf->out);
    }
    return lpf->out;
}

// --- 电源模式枚举 ---
typedef enum {
    MODE_BUCK = 0,
    MODE_BOOST
} PowerMode_t;

// --- 滞环带宽 ---
#define HYST_BAND_V     0.8f

// --- PI 参数结构体 ---
typedef struct {
    float kp;
    float ki;
    float integral;
} PI_Controller_t;

// --- Buck / Boost 独立 PI 控制器 ---
static PI_Controller_t buck_pi  = {0.01f, 15.0f, 0.0f};
static PI_Controller_t boost_pi = {0.01f, 15.0f, 0.0f};

// --- 恒流环 (CC) PI 控制器 ---
static PI_Controller_t cc_pi = {0.2f, 20.0f, 0.0f};

static PowerMode_t current_mode = MODE_BUCK;
static uint8_t uvlo_fault = 0;

// --- PWM 参数 ---
#define PWM_RESOLUTION_MAX        13599.0f
#define BOOTSTRAP_MIN_RECHARGE_RATIO 0.10f

void Update_Buck_Duty(float duty) {
    if (duty > (1.0f - BOOTSTRAP_MIN_RECHARGE_RATIO)) {
        duty = 1.0f - BOOTSTRAP_MIN_RECHARGE_RATIO;
    }
    if (duty < 0.0f) duty = 0.0f;
    uint32_t ccr_value = (uint32_t)(PWM_RESOLUTION_MAX * (1.0f - duty));
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, ccr_value);
}

void Update_Boost_Duty(float duty) {
    if (duty > (1.0f - BOOTSTRAP_MIN_RECHARGE_RATIO)) {
        duty = 1.0f - BOOTSTRAP_MIN_RECHARGE_RATIO;
    }
    if (duty < 0.0f) duty = 0.0f;
    uint32_t ccr_value = (uint32_t)(PWM_RESOLUTION_MAX * duty);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, ccr_value);
}

// --- 全局变量 ---
PowerMeas_t powerMeas = {0};
PowerState_t powerState = {0};

#define ADC1_CH_NUM 2
#define ADC2_CH_NUM 4
#define BUF_DEPTH   4

uint16_t adc_buffer1[ADC1_CH_NUM * BUF_DEPTH];
uint16_t adc_buffer2[ADC2_CH_NUM * BUF_DEPTH];
float adc_voltages[6];

// --- 本地静态变量 ---
static const float Ts = 0.0001f;       // 10kHz 采样周期
static FirstOrderLPF_t vout_lpf = {0, 0, 0};
static FirstOrderLPF_t vin_lpf = {0, 0, 0};
static FirstOrderLPF_t iin_lpf = {0, 0, 0};
static uint8_t ctrl_initialized = 0;
static uint32_t startup_counter = 0;
static uint8_t is_power_ready = 0;

#define STARTUP_DELAY_MS    20.0f
#define STARTUP_DELAY_TICKS (uint32_t)(STARTUP_DELAY_MS / (Ts * 1000.0f))

// --- 初始化 ---
void Control_Init(void) {
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer1, ADC1_CH_NUM * BUF_DEPTH);
    HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc_buffer2, ADC2_CH_NUM * BUF_DEPTH);
    powerState.target_v = 12.0f;
    powerState.target_i = 3.0f;
    powerState.pi_ki = 150.0f;
    powerState.pi_kp = 0.15f;
}

// --- DMA 中断回调 ---
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        for (int ch = 0; ch < ADC1_CH_NUM; ch++) {
            uint32_t sum = 0;
            for (int i = 0; i < BUF_DEPTH; i++) {
                sum += adc_buffer1[i * ADC1_CH_NUM + ch];
            }
            adc_voltages[ch] = ((float)sum / BUF_DEPTH) * 3.3f / 65535.0f * 2.0f;
        }
    }
    else if (hadc->Instance == ADC2) {
        for (int ch = 0; ch < ADC2_CH_NUM; ch++) {
            uint32_t sum = 0;
            for (int i = 0; i < BUF_DEPTH; i++) {
                sum += adc_buffer2[i * ADC2_CH_NUM + ch];
            }
            adc_voltages[ADC1_CH_NUM + ch] = ((float)sum / BUF_DEPTH) * 3.3f / 65535.0f * 2.0f;
        }
    }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC2) {
        (void)HAL_ADC_GetError(hadc);
    }
}

// --- PWM 安全接管函数 ---
void Power_Set_Safe_PWM(void) {
    Update_Buck_Duty(0.0f);
    Update_Boost_Duty(0.0f);
}

// --- 主控制 tick ---
void Control_Tick_Hook(void) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);

    // 1. 初始化（仅执行一次）
    if (!ctrl_initialized) {
        LPF_CalcAlpha(&vout_lpf, 1500.0f, Ts);
        LPF_CalcAlpha(&vin_lpf, 1500.0f, Ts);
        LPF_CalcAlpha(&iin_lpf, 500.0f, Ts);
        ctrl_initialized = 1;
    }

    // 2. ADC 换算
    //powerMeas.iin = (adc_voltages[3] - 1.65f) * 200.0f / 100.0f;
    powerMeas.temp = adc_voltages[5];
    powerMeas.inductor_i = adc_voltages[0]; // 20.0f / 100.0f;
    powerMeas.iout = -(adc_voltages[1] - 1.608f) * 200.0f / 100.0f * 0.9029f - 0.035716f;

    float raw_vout = adc_voltages[2] * 12.0f;
    float raw_vin  = adc_voltages[4] * 12.0f + 0.0965f;
    float raw_iin = (adc_voltages[3] - 1.65f) * 200.0f / 100.0f;
    powerMeas.vout = LPF_Update(&vout_lpf, raw_vout);
    powerMeas.vin  = LPF_Update(&vin_lpf, raw_vin);
    powerMeas.iin  = LPF_Update(&iin_lpf, raw_iin);

    float target_v = powerState.target_v;
    float vin = powerMeas.vin;
    float current_v = powerMeas.vout;

    // ==========================================
    // 3. UVLO 欠压保护逻辑 (带滞环)
    // ==========================================
    if (!uvlo_fault) {
        if (vin < 5.0f) {
            uvlo_fault = 1;
        }
    } else {
        if (vin > 6.0f) {
            uvlo_fault = 0;
            startup_counter = 0;
            is_power_ready = 0;
            buck_pi.integral = 0.0f;
            boost_pi.integral = 0.0f;
            cc_pi.integral = 0.0f;
        }
    }

    if (uvlo_fault) {
        Power_Set_Safe_PWM();
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
        return;
    }

    // ==========================================
    // 4. 软启动等待
    // ==========================================
    if (startup_counter < STARTUP_DELAY_TICKS) {
        startup_counter++;
        Update_Buck_Duty(0.0f);
        Update_Boost_Duty(0.1f);

        buck_pi.integral = 0.0f;
        boost_pi.integral = 0.0f;
        cc_pi.integral = 0.0f;
        is_power_ready = 0;

        if (powerMeas.vin > powerState.target_v) {
            current_mode = MODE_BUCK;
        } else {
            current_mode = MODE_BOOST;
        }
        return;
    } else {
        is_power_ready = 1;
    }

    float target_i = powerState.target_i;
    float current_i = powerMeas.iout;

    // ==========================================
    // 5. 恒流环 (CC Loop) 计算
    // ==========================================
    float err_i = current_i - target_i;

    if (err_i > 0.0f || cc_pi.integral > 0.0f) {
        cc_pi.integral += cc_pi.ki * err_i * Ts;
        if (cc_pi.integral < 0.0f) cc_pi.integral = 0.0f;
        if (cc_pi.integral > target_v) cc_pi.integral = target_v;
    }

    float dynamic_target_v = target_v - cc_pi.integral;
    if (err_i > 0.0f) {
        dynamic_target_v -= cc_pi.kp * err_i;
    }
    if (dynamic_target_v < 0.0f) dynamic_target_v = 0.0f;

    float err = dynamic_target_v - current_v;

    // ==========================================
    // 6. 滞环状态机
    // ==========================================
    float switch_to_boost_threshold = target_v + 0.6f;
    float switch_to_buck_threshold  = switch_to_boost_threshold + HYST_BAND_V;

    if (current_mode == MODE_BUCK) {
        if (vin < switch_to_boost_threshold) {
            current_mode = MODE_BOOST;
            float expected_duty = 1.0f - ((vin * 0.9f) / target_v);
            if (expected_duty < 0.1f) expected_duty = 0.1f;
            if (expected_duty > 0.5f) expected_duty = 0.5f;
            boost_pi.integral = expected_duty;
        }
    } else {
        if (vin > switch_to_buck_threshold) {
            current_mode = MODE_BUCK;
            buck_pi.integral = 0.85f;
        }
    }

    // ==========================================
    // 7. 双模独立 PI 控制
    // ==========================================
    if (current_mode == MODE_BUCK) {
        buck_pi.integral += buck_pi.ki * err * Ts;
        if (buck_pi.integral > 0.85f) buck_pi.integral = 0.85f;
        if (buck_pi.integral < 0.0f)  buck_pi.integral = 0.0f;

        float duty_out = (buck_pi.kp * err) + buck_pi.integral;
        if (duty_out > 0.9f) duty_out = 0.9f;
        if (duty_out < 0.0f) duty_out = 0.0f;

        if (is_power_ready) {
            Update_Buck_Duty(duty_out);
            Update_Boost_Duty(0.1f);
        }
    } else {
        boost_pi.integral += boost_pi.ki * err * Ts;
        if (boost_pi.integral > 0.80f) boost_pi.integral = 0.80f;
        if (boost_pi.integral < 0.0f)  boost_pi.integral = 0.0f;

        float duty_out = (boost_pi.kp * err) + boost_pi.integral;
        if (duty_out > 0.85f) duty_out = 0.85f;
        if (duty_out < 0.0f)  duty_out = 0.0f;

        if (is_power_ready) {
            Update_Buck_Duty(0.9f);
            Update_Boost_Duty(duty_out);
        }
    }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
}