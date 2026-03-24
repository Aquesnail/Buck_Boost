#include "button.h"
#include <stddef.h>

static Button_t *g_btns = NULL;
static uint8_t g_btn_count = 0;

void Button_Core_Init(Button_t *btns, uint8_t count) {
    g_btns = btns;
    g_btn_count = count;
}

// 内部状态机处理逻辑（简化版示意）
static void _Button_Handler(Button_t *btn, uint16_t tick_ms) {
    uint8_t cur_level = btn->read_pin_level(btn->id);
    uint8_t is_pressed = (cur_level == btn->active_level);

    switch (btn->state) {
        case 0: // 待机态
            if (is_pressed) {
                btn->state = 1; // 进入消抖
                btn->ticks = 0;
            }
            break;

        case 1: // 消抖态
            if (is_pressed) {
                btn->state = 2; // 确认按下
                btn->event_callback(btn->id, BTN_EVENT_SINGLE_CLICK); // 示例：直接触发
            } else {
                btn->state = 0;
            }
            break;

        case 2: // 释放态
            if (!is_pressed) {
                btn->state = 0;
                btn->event_callback(btn->id, BTN_EVENT_RELEASE);
            }
            break;
    }
}

void Button_Core_Tick(uint16_t tick_ms) {
    if (g_btns == NULL) return;

    for (uint8_t i = 0; i < g_btn_count; i++) {
        Button_t *b = &g_btns[i];
        uint8_t cur_level = b->read_pin_level(b->id);

        // 简易静态消抖状态机
        if (cur_level == b->active_level) { 
            // 按下状态
            if (b->debounce_cnt < 3) { // 连续 3 次(30ms)确认
                b->debounce_cnt++;
            } else if (b->state == 0) {
                b->state = 1; // 标记为已按下
                if (b->event_callback) 
                    b->event_callback(b->id, BTN_EVENT_SINGLE_CLICK);
            }
        } else {
            // 松开状态
            b->debounce_cnt = 0;
            if (b->state == 1) {
                b->state = 0;
                if (b->event_callback)
                    b->event_callback(b->id, BTN_EVENT_RELEASE);
            }
        }
    }
}