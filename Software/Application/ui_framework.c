#include "ui_framework.h"
#include "encoder_port.h"
#include "ST7789_SPI_Port.h"
#include "ST7789.h"

/* 心跳分频目标周期：假设 UI_Tick_Handler 每 1ms 被调用一次 */
#ifndef UI_TICK_PERIOD_MS
#define UI_TICK_PERIOD_MS  1
#endif

static UI_Page_t **page_table = NULL;
static uint8_t    page_count = 0;
static uint8_t    curr_idx   = 0;

static volatile uint8_t    btn_pending = 0;
static volatile uint8_t    btn_id_buf;
static volatile ButtonEvent_t btn_event_buf;

void UI_Init(void) {
    page_table = NULL;
    page_count = 0;
    curr_idx = 0;
    btn_pending = 0;
    LCD_Clear(ST7789_COLOR_BLACK);
}

void UI_RegisterPages(UI_Page_t *pages[], uint8_t count) {
    page_table = pages;
    page_count = count;
    for (uint8_t i = 0; i < count; i++) {
        if (pages[i]->Init) pages[i]->Init();
    }
    if (count > 0 && pages[0]->Enter) {
        pages[0]->Enter();
    }
}

void UI_SwitchPage(uint8_t page_idx) {
    if (page_table == NULL || page_idx >= page_count || page_idx == curr_idx) return;
    if (page_table[curr_idx]->Exit) page_table[curr_idx]->Exit();
    curr_idx = page_idx;
    if (page_table[curr_idx]->Enter) page_table[curr_idx]->Enter();
}

uint8_t UI_GetCurrentPageIdx(void) {
    return curr_idx;
}

void UI_NotifyButton(uint8_t btn_id, ButtonEvent_t event) {
    btn_id_buf = btn_id;
    btn_event_buf = event;
    btn_pending = 1;
}

void UI_Tick_Handler(void) {
    static uint8_t divider = 0;
    if (++divider < UI_TICK_PERIOD_MS) return;
    divider = 0;

    if (page_table == NULL) return;
    UI_Page_t *page = page_table[curr_idx];
    int16_t delta = Encoder_Get_Delta();

    /* 分发按键事件 */
    if (btn_pending) {
        btn_pending = 0;
        if (page->OnButton) {
            page->OnButton(btn_id_buf, btn_event_buf);
        }
    }

    /* 分发心跳 */
    if (page->Tick) {
        page->Tick(delta);
    }
}

void UI_Draw_Handler(void) {
    if (page_table == NULL) return;
    UI_Page_t *page = page_table[curr_idx];
    if (page->Draw) {
        page->Draw();
    }
}
