#include "ui_page_control.h"
#include <stdio.h>
#include <string.h>
#include "ST7789_SPI_Port.h"
#include "ST7789.h"
#include "ui_data.h"
#include "font.h"

static void Page_Control_Init(void) {}
static void Page_Control_Enter(void) {}
static void Page_Control_Exit(void) {}
static void Page_Control_Tick(int16_t delta) { (void)delta; }
static void Page_Control_OnButton(uint8_t btn_id, ButtonEvent_t event) {
    (void)btn_id;
    (void)event;
    /* 预留：短按右键/下键切换页面等 */
}
static void Page_Control_Draw(void) {
    /* 最小占位实现：黑屏显示页面名称 */
    LCD_DrawString(10, 10, "Control Page", &afont16x8,
                   ST7789_COLOR_WHITE, ST7789_COLOR_BLACK);
}

UI_Page_t ui_page_control = {
    .name = "Control",
    .Init = Page_Control_Init,
    .Enter = Page_Control_Enter,
    .Exit = Page_Control_Exit,
    .Tick = Page_Control_Tick,
    .OnButton = Page_Control_OnButton,
    .Draw = Page_Control_Draw,
};
