#ifndef __UI_DATA_H__
#define __UI_DATA_H__

#include <stdint.h>

/* ==========================================================================
 * 电源实时测量数据（由 control / ADC 写入，UI 只读）
 * ========================================================================== */
typedef struct {
    float vin;          // 输入电压 (V)
    float vout;         // 输出电压 (V)
    float iin;          // 输入电流 (A)
    float iout;         // 输出电流 (A)
    float temp;         // 温度 (°C)
    float inductor_i;       // 电感电流 (A)
} PowerMeas_t;

/* ==========================================================================
 * 电源控制状态（UI 可读写设定值，control 读取执行）
 * ========================================================================== */
typedef struct {
    float target_v;     // 设定输出电压 (V)  — CC 模式下为合规电压上限
    float target_i;     // 设定限流电流 (A)  — CC 模式下为注入电流设定值
    float pi_kp;        // 电压环 PI Kp
    float pi_ki;        // 电压环 PI Ki
    uint8_t output_en;  // 输出使能标志
    uint8_t cc_mode;    // 恒流注入模式标志 (0=CV, 1=CC)
} PowerState_t;

/* ==========================================================================
 * 全局实例声明（在 control.c 中定义）
 * ========================================================================== */
extern PowerMeas_t powerMeas;
extern PowerMeas_t powerMeasDisp;
extern PowerState_t powerState;

#endif
