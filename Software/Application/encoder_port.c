#include "encoder_port.h"
#include "tim.h"

#define ENCODER_MID_VALUE  32768u

static uint16_t last_cnt = ENCODER_MID_VALUE;

void Encoder_Init(void) {
    __HAL_TIM_SET_COUNTER(&htim1, ENCODER_MID_VALUE);
    last_cnt = ENCODER_MID_VALUE;
}

uint16_t Encoder_Get_Position(void) {
    return (uint16_t)__HAL_TIM_GET_COUNTER(&htim1);
}

int16_t Encoder_Get_Delta(void) {
    uint16_t curr = (uint16_t)__HAL_TIM_GET_COUNTER(&htim1);
    int16_t delta = (int16_t)(curr - last_cnt);
    last_cnt = curr;
    return delta;
}
