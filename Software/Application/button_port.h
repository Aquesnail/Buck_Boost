#ifndef __BUTTON_PORT_H__
#define __BUTTON_PORT_H__

#include "button.h"

#define BTN_HW_COUNT  4 

void Button_Port_Init(void);
void Button_Port_Tick_Handler(void);

#endif