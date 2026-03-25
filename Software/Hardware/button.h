#ifndef __BUTTON_H__
#define __BUTTON_H__

#include <stdint.h>

// 按键触发事件类型
typedef enum {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_SINGLE_CLICK,      // 单击确认 (在双击判定超时后触发)
    BTN_EVENT_DOUBLE_CLICK,      // 双击
    BTN_EVENT_LONG_PRESS_START,  // 长按开始触发
    BTN_EVENT_LONG_PRESS_HOLD,   // 长按期间循环触发
    BTN_EVENT_RELEASE,           // 按键释放
} ButtonEvent_t;

// 按键结构体
typedef struct Button {
    /* --- 配置参数 --- */
    uint8_t id;
    uint8_t active_level;    // 有效电平 (0 或 1)
    uint8_t (*read_pin_level)(uint8_t id); 
    void (*event_callback)(struct Button *btn, ButtonEvent_t event);
    void *user_data;         // 用户自定义上下文指针（如指向某个 UI 对象或结构体）

    /* --- 内部状态机变量 --- */
    uint8_t  state;           
    uint8_t  debounce_cnt;    
    uint16_t ticks;           
    uint8_t  click_cnt;       
} Button_t;

/* 核心 API */
void Button_Core_Init(Button_t *btns, uint8_t count);
void Button_Core_Tick(uint16_t tick_ms);

#endif