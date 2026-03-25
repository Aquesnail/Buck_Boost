#include "button.h"
#include <stddef.h>

/* 时间参数常量定义 (单位: ms) */
#define BTN_DEBOUNCE_MS     20    // 消抖时间
#define BTN_LONG_PRESS_MS   800   // 长按判定阈值
#define BTN_DOUBLE_CLICK_MS 250   // 双击判定间隔
#define BTN_HOLD_TICK_MS    100   // 长按保持触发频率

static Button_t *g_btns = NULL;
static uint8_t g_btn_count = 0;

void Button_Core_Init(Button_t *btns, uint8_t count) {
    g_btns = btns;
    g_btn_count = count;
    for (uint8_t i = 0; i < count; i++) {
        btns[i].state = 0;
        btns[i].ticks = 0;
        btns[i].click_cnt = 0;
    }
}

void Button_Core_Tick(uint16_t tick_ms) {
    if (g_btns == NULL) return;

    for (uint8_t i = 0; i < g_btn_count; i++) {
        Button_t *b = &g_btns[i];
        if (!b->read_pin_level || !b->event_callback) continue;

        uint8_t is_pressed = (b->read_pin_level(b->id) == b->active_level);

        switch (b->state) {
            case 0: // 【待机/双击等待】
                if (is_pressed) {
                    b->ticks = 0;
                    b->state = 1; // 去消抖
                } else if (b->click_cnt > 0) {
                    b->ticks += tick_ms;
                    if (b->ticks > BTN_DOUBLE_CLICK_MS) {
                        // 双击判定超时，根据点击次数触发对应事件
                        b->event_callback(b, (b->click_cnt == 1) ? BTN_EVENT_SINGLE_CLICK : BTN_EVENT_DOUBLE_CLICK);
                        b->click_cnt = 0;
                    }
                }
                break;

            case 1: // 【消抖确认】
                b->ticks += tick_ms;
                if (is_pressed) {
                    if (b->ticks >= BTN_DEBOUNCE_MS) {
                        b->state = 2; // 确认按下
                        b->ticks = 0;
                    }
                } else {
                    b->state = 0; 
                }
                break;

            case 2: // 【按下中/判定长按】
                if (is_pressed) {
                    b->ticks += tick_ms;
                    if (b->ticks >= BTN_LONG_PRESS_MS) {
                        b->event_callback(b, BTN_EVENT_LONG_PRESS_START);
                        b->state = 3; // 转入长按保持
                        b->ticks = 0;
                        b->click_cnt = 0; // 长按清除连击计数
                    }
                } else {
                    // 短促按下并释放
                    b->click_cnt++;
                    b->ticks = 0; 
                    b->event_callback(b, BTN_EVENT_RELEASE);
                    b->state = 0; // 回到状态 0 等待双击或单击超时
                }
                break;

            case 3: // 【长按保持触发】
                if (is_pressed) {
                    b->ticks += tick_ms;
                    if (b->ticks >= BTN_HOLD_TICK_MS) {
                        b->event_callback(b, BTN_EVENT_LONG_PRESS_HOLD);
                        b->ticks = 0;
                    }
                } else {
                    b->event_callback(b, BTN_EVENT_RELEASE);
                    b->state = 0;
                }
                break;
        }
    }
}