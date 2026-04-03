#include "ui_page_output.h"
#include <stdio.h>
#include <string.h>
#include "ST7789_SPI_Port.h"
#include "ST7789.h"
#include "ui_data.h"
#include "font.h"

/* 屏幕和字体占位参数（后续更换字体时只需修改此处宏） */
#define SCREEN_W      320
#define SCREEN_H      240
#define VAL_FONT      (&afont24x12)
#define VAL_FONT_H    24
#define VAL_FONT_W    12
#define LABEL_FONT    (&afont16x8)
#define LABEL_FONT_H  16
#define LABEL_FONT_W  8

/* 编辑步进 */
#define V_STEP        0.05f
#define I_STEP        0.01f

/* 最大值限制 */
#define V_MAX         30.0f
#define I_MAX         10.0f

typedef enum {
    OUT_MODE_IDLE = 0,
    OUT_MODE_EDIT_V,
    OUT_MODE_EDIT_I,
} OutputEditMode_t;

static OutputEditMode_t edit_mode = OUT_MODE_IDLE;

/* 绘制辅助：绘制一个数值字符串（右对齐） */
static void DrawValueRight(uint16_t x, uint16_t y, const char *str,
                           const ASCIIFont *font, uint16_t fg, uint16_t bg,
                           uint8_t max_chars) {
    uint16_t w = strlen(str) * font->w;
    uint16_t max_w = max_chars * font->w;
    uint16_t start_x = (x + max_w > w) ? (x + max_w - w) : x;
    LCD_DrawString(start_x, y, str, font, fg, bg);
}

static void Page_Output_Init(void) {
    edit_mode = OUT_MODE_IDLE;
}

static void Page_Output_Enter(void) {
    edit_mode = OUT_MODE_IDLE;
}

static void Page_Output_Exit(void) {
    edit_mode = OUT_MODE_IDLE;
}

static void Page_Output_Tick(int16_t delta) {
    if (delta == 0) return;

    switch (edit_mode) {
        case OUT_MODE_EDIT_V:
            powerState.target_v += (float)delta * V_STEP;
            if (powerState.target_v < 0.0f) powerState.target_v = 0.0f;
            if (powerState.target_v > V_MAX) powerState.target_v = V_MAX;
            break;
        case OUT_MODE_EDIT_I:
            powerState.target_i += (float)delta * I_STEP;
            if (powerState.target_i < 0.0f) powerState.target_i = 0.0f;
            if (powerState.target_i > I_MAX) powerState.target_i = I_MAX;
            break;
        default:
            break;
    }
}

static void Page_Output_OnButton(uint8_t btn_id, ButtonEvent_t event) {
    if (btn_id != 0) return; /* 只响应编码器按键 */
    if (event != BTN_EVENT_SINGLE_CLICK) return;

    switch (edit_mode) {
        case OUT_MODE_IDLE:
            edit_mode = OUT_MODE_EDIT_V;
            break;
        case OUT_MODE_EDIT_V:
            edit_mode = OUT_MODE_EDIT_I;
            break;
        case OUT_MODE_EDIT_I:
            edit_mode = OUT_MODE_IDLE;
            break;
        default:
            break;
    }
}

static void Page_Output_Draw(void) {
    uint16_t bg = ST7789_COLOR_BLACK;
    uint16_t fg = ST7789_COLOR_WHITE;
    uint16_t fg_hl = ST7789_COLOR_BLACK;
    uint16_t bg_hl = ST7789_COLOR_YELLOW;

    /* 根据编辑状态选择颜色 */
    uint16_t v_fg = (edit_mode == OUT_MODE_EDIT_V) ? fg_hl : fg;
    uint16_t v_bg = (edit_mode == OUT_MODE_EDIT_V) ? bg_hl : bg;
    uint16_t i_fg = (edit_mode == OUT_MODE_EDIT_I) ? fg_hl : fg;
    uint16_t i_bg = (edit_mode == OUT_MODE_EDIT_I) ? bg_hl : bg;

    /* 第一排：电压设定/输出 */
    LCD_DrawString(10, 60, "Vset:", LABEL_FONT, fg, bg);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", (float)powerState.target_v);
    DrawValueRight(80, 55, buf, VAL_FONT, v_fg, v_bg, 6);
    LCD_DrawString(220, 60, "V", LABEL_FONT, fg, bg);

    /* 第二排：电流设定/限流 */
    LCD_DrawString(10, 140, "Iset:", LABEL_FONT, fg, bg);
    snprintf(buf, sizeof(buf), "%.3f", (float)powerState.target_i);
    DrawValueRight(80, 135, buf, VAL_FONT, i_fg, i_bg, 6);
    LCD_DrawString(220, 140, "A", LABEL_FONT, fg, bg);
}

UI_Page_t ui_page_output = {
    .name = "Output",
    .Init = Page_Output_Init,
    .Enter = Page_Output_Enter,
    .Exit = Page_Output_Exit,
    .Tick = Page_Output_Tick,
    .OnButton = Page_Output_OnButton,
    .Draw = Page_Output_Draw,
};
