#include "control.h"
#include "stm32g431xx.h"
#include "tim.h"
#include "stm32g4xx_hal_tim.h"
#include "ui_data.h"
#include <stdint.h>

PowerMeas_t powerMeas = {0};
PowerState_t powerState = {0};

void Control_Init(void){
    powerState.target_i = 1.22f;
    powerState.target_v = 1.552f;
}

void Control_Tick_Hook(void){
    
    powerState.pi_ki = 1.0f;
    powerState.pi_kp = 1.0f;
    powerState.output_en = 1;
}


