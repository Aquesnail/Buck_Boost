#ifndef __BUTTON_PORT_H__
#define __BUTTON_PORT_H__

#include "button.h"

// 根据图片，共有 4 个按键
#define BTN_HW_COUNT  4 

/* 初始化接口 */
void Button_Port_Init(void);

/* 滴答中断钩子，需放在 stm32xxxx_it.c 的 SysTick_Handler 中 */
void Button_Port_Tick_Handler(void);

#endif