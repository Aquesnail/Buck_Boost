#ifndef __ENCODER_PORT_H__
#define __ENCODER_PORT_H__

#include <stdint.h>

/*
 * @brief 初始化编码器接口
 * @note  调用前需确保 HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL) 已执行
 */
void Encoder_Init(void);

/*
 * @brief 获取编码器当前绝对位置（0~65535）
 */
uint16_t Encoder_Get_Position(void);

/*
 * @brief 获取编码器相对于上一次调用的变化量（有符号）
 * @retval 正数:顺时针, 负数:逆时针
 */
int16_t Encoder_Get_Delta(void);

#endif
