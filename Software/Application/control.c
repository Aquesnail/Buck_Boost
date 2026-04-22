#include "control.h"
#include "stm32g431xx.h"
#include "tim.h"
#include "stm32g4xx_hal_tim.h"
#include "ui_data.h"
#include <stdint.h>
#include "adc.h"

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

void Control_Tick_Hook(void){
    
    powerState.pi_ki = 1.0f;
    powerState.pi_kp = 1.0f;
    powerState.output_en = 1;

    powerMeas.vout = adc_voltages[2]*12.0f;
    powerMeas.iin = (adc_voltages[3]-1.65f)*20.0f/100.0f;
    powerMeas.vin = adc_voltages[4]*12.0f;
    powerMeas.temp = adc_voltages[5];

    powerMeas.inductor_i = adc_voltages[0]*20.0f/100.0f;
    powerMeas.iout = adc_voltages[1]*20.0f/100.0f;
    powerState.target_i = 1.22f;
    powerState.target_v = 1.552f;
}


void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC2) {
        // 如果运行到这里，说明 ADC2 溢出或者配置冲突了
        // 常见的错误码是 HAL_ADC_ERROR_DMA
        uint32_t err = HAL_ADC_GetError(hadc);
    }
}