#ifndef _CONTROL_H
#define _CONTROL_H

#include <stdint.h>

void Control_Tick_Hook(void);
void Control_Init(void);

void Power_Set_Safe_PWM(void);
#endif