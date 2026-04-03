#include "control.h"
#include "stm32g431xx.h"
#include "tim.h"
#include "stm32g4xx_hal_tim.h"
#include "power_ui_api.h"
#include <stdint.h>

PowerMeas_t powerMeas={0};
PowerState_t powerState={0};



void Control_Tick_Hook(void){
    PowerAPI_SetTargetI(1.22);
   
}


