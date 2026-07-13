#ifndef _CONTROL_H
#define _CONTROL_H

#include <stdint.h>

void Control_Tick_Hook(void);
void Control_Init(void);
void Power_Set_Safe_PWM(void);
void Power_Set_OpenLoop_Duty(int32_t ccr_unified);
void Power_Exit_OpenLoop(void);

extern float buck_duty;
extern float boost_duty;
extern float inv_one_minus_d;
#endif