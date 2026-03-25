#include "power_ui_api.h"
// 如果你用了 FreeRTOS，这里就包含 FreeRTOS.h 和 task.h
// 如果是裸机中断，这里可以包含主控的关中断宏定义

// 静态的内部数据，只有这个 C 文件可以访问，彻底隔绝外部全局引用
static PowerMeas_t  s_meas_data = {0};
static PowerState_t s_state_data = {
    .V_set = 5.0f,     // 默认上电 5V
    .I_set = 1.0f,     // 默认上电 1A
    .out_enable = 0,
    .mode = SYS_STANDBY
};

// =====================================
// 底层控制任务调用的接口 (供内部 ADC 或 PID 任务更新数据)
// =====================================
void PowerCtrl_UpdateMeasurements(PowerMeas_t *new_meas) {
    // 进入临界区 (屏蔽中断或挂起调度器)
    // taskENTER_CRITICAL(); // FreeRTOS 用法
    
    s_meas_data = *new_meas; // 快速结构体拷贝
    s_meas_data.Pout = s_meas_data.Vout * s_meas_data.Iout; // 功率可以在这里统一算
    
    // 退出临界区
    // taskEXIT_CRITICAL();
}

// =====================================
// 给 UI 调用的 Getter 接口
// =====================================
void PowerAPI_GetMeasurements(PowerMeas_t *meas_out) {
    // taskENTER_CRITICAL();
    *meas_out = s_meas_data;
    // taskEXIT_CRITICAL();
}

void PowerAPI_GetState(PowerState_t *state_out) {
    // taskENTER_CRITICAL();
    *state_out = s_state_data;
    // taskEXIT_CRITICAL();
}

// =====================================
// 给 UI 调用的 Setter 接口
// =====================================
void PowerAPI_SetTargetV(float v_set) {
    // 可以加一些软限幅保护
    if(v_set > 24.0f) v_set = 24.0f; 
    if(v_set < 0.0f)  v_set = 0.0f;
    
    // taskENTER_CRITICAL();
    s_state_data.V_set = v_set;
    // taskEXIT_CRITICAL();
}

void PowerAPI_SetTargetI(float i_set) {
    if(i_set > 5.0f) i_set = 5.0f;
    if(i_set < 0.0f) i_set = 0.0f;
    
    // taskENTER_CRITICAL();
    s_state_data.I_set = i_set;
    // taskEXIT_CRITICAL();
}

void PowerAPI_SetOutputEnable(uint8_t enable) {
    // taskENTER_CRITICAL();
    s_state_data.out_enable = enable;
    if (enable == 0) {
        s_state_data.mode = SYS_STANDBY;
    }
    // taskEXIT_CRITICAL();
    
    // 这里还可以触发一个事件标志或信号量，立刻通知底层硬件关闭 PWM
}