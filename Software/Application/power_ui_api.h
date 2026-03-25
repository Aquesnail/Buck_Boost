#ifndef __POWER_UI_API_H
#define __POWER_UI_API_H

#include "stdint.h"

// 1. 系统工作状态枚举
typedef enum {
    SYS_STANDBY = 0, // 待机 (关闭输出)
    SYS_RUN_CV,      // 运行中 (恒压模式)
    SYS_RUN_CC,      // 运行中 (恒流模式)
    SYS_FAULT        // 故障保护状态
} PowerSysMode_e;

// 2. 测量值结构体 (UI 只能读，不能改)
typedef struct {
    float Vin;
    float Vout;
    float Iin;
    float Iout;
    float Pout;
    float IL_avg;  // 电感电流平均值 (底层控制循环负责平滑滤波后传给这个变量)
    float temp_c;  // 温度 (备用)
} PowerMeas_t;

// 3. 设定值与状态结构体 (UI 可读可写)
typedef struct {
    float V_set;       // 目标电压设定值
    float I_set;       // 目标电流设定值
    uint8_t out_enable;// 输出使能标志 (1:开启, 0:关闭)
    PowerSysMode_e mode; // 当前系统状态
} PowerState_t;


// =====================================
// UI 获取数据的接口 (Getter)
// =====================================
// UI 调用此函数获取最新的测量数据
void PowerAPI_GetMeasurements(PowerMeas_t *meas_out);

// UI 调用此函数获取当前的系统状态和设定值
void PowerAPI_GetState(PowerState_t *state_out);

// =====================================
// UI 下发指令的接口 (Setter)
// =====================================
// 编码器调节电压后，调用此函数同步给底层
void PowerAPI_SetTargetV(float v_set);

// 编码器调节电流后，调用此函数同步给底层
void PowerAPI_SetTargetI(float i_set);

// 按下 BTN1 开关机时，调用此函数
void PowerAPI_SetOutputEnable(uint8_t enable);

#endif