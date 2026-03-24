#ifndef __BUTTON_H__
#define __BUTTON_H__

#include <stdint.h>

// 按键触发事件类型
typedef enum {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_SINGLE_CLICK,
    BTN_EVENT_DOUBLE_CLICK,
    BTN_EVENT_LONG_PRESS_START,
    BTN_EVENT_LONG_PRESS_HOLD,
    BTN_EVENT_RELEASE,
} ButtonEvent_t;

// 按键结构体
typedef struct Button {
    /* 硬件关联 */
    uint8_t id;
    uint8_t (*read_pin_level)(uint8_t id); // 返回 0:低电平, 1:高电平
    void (*event_callback)(uint8_t id, ButtonEvent_t event);

    /* 内部状态机变量 */
    uint8_t  state;           // 当前状态
    uint8_t  debounce_cnt;    // 消抖计数
    uint16_t ticks;           // 计时器
    uint8_t  click_cnt;       // 点击次数记录
    uint8_t  active_level;    // 有效电平 (0 或 1)
} Button_t;

/* 核心 API */
void Button_Core_Init(Button_t *btns, uint8_t count);
void Button_Core_Tick(uint16_t tick_ms);

#endif