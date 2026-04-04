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

/* 数值固定位数: XX.XXX */
#define DIGIT_COUNT   5

/* 钳位值 (显示范围必须与按位编辑对齐) */
#define V_MAX         36.0f
#define I_MAX         5.0f

typedef enum {
    OUT_MODE_IDLE = 0,
    OUT_MODE_EDIT_V,
    OUT_MODE_EDIT_I,
} OutputEditMode_t;

static OutputEditMode_t edit_mode = OUT_MODE_IDLE;
static uint8_t digit_idx = 1; /* 默认个位 */

static const int32_t digit_multipliers[DIGIT_COUNT] = {
    10000, 1000, 100, 10, 1
};

/* ==========================================================================
 * 内部辅助函数
 * ========================================================================== */

static void DrawDigitBg(uint16_t x, uint16_t y,
                        const ASCIIFont *font, uint16_t bg,
                        uint8_t selected, uint16_t box_color) {
    uint16_t pad = 1;
    uint16_t r   = 2;
    uint16_t box_x = (x >= pad) ? (x - pad) : 0;
    uint16_t box_y = (y >= pad) ? (y - pad) : 0;
    uint16_t box_w = font->w + pad * 2;
    uint16_t box_h = font->h + pad * 2;

    if (selected) {
        LCD_FillRoundRect(box_x, box_y, box_w, box_h, r, box_color);
    } else {
        LCD_Fill(x, y, font->w, font->h, bg);
    }
}

static void DrawValueFixed(uint16_t x, uint16_t y, float value,
                           uint8_t sel_digit, uint16_t fg, uint16_t bg,
                           uint16_t box_color) {
    int32_t milli = (int32_t)(value * 1000.0f + 0.5f);
    if (milli < 0) milli = 0;

    uint8_t digits[DIGIT_COUNT];
    for (int i = 0; i < DIGIT_COUNT; i++) {
        digits[i] = (uint8_t)((milli / digit_multipliers[i]) % 10);
    }

    uint16_t cx = x;

    /* 第1阶段：画所有背景和框 */
    DrawDigitBg(cx, y, VAL_FONT, bg, sel_digit == 0, box_color); cx += VAL_FONT_W;
    DrawDigitBg(cx, y, VAL_FONT, bg, sel_digit == 1, box_color); cx += VAL_FONT_W;
    LCD_Fill(cx, y, VAL_FONT_W, VAL_FONT_H, bg);                 cx += VAL_FONT_W; /* 小数点 */
    DrawDigitBg(cx, y, VAL_FONT, bg, sel_digit == 2, box_color); cx += VAL_FONT_W;
    DrawDigitBg(cx, y, VAL_FONT, bg, sel_digit == 3, box_color); cx += VAL_FONT_W;
    DrawDigitBg(cx, y, VAL_FONT, bg, sel_digit == 4, box_color); cx += VAL_FONT_W;

    /* 第2阶段：只画前景字符 */
    cx = x;
    LCD_DrawChar_NoBg(cx, y, '0' + digits[0], VAL_FONT, fg); cx += VAL_FONT_W;
    LCD_DrawChar_NoBg(cx, y, '0' + digits[1], VAL_FONT, fg); cx += VAL_FONT_W;
    LCD_DrawChar_NoBg(cx, y, '.', VAL_FONT, fg);             cx += VAL_FONT_W;
    LCD_DrawChar_NoBg(cx, y, '0' + digits[2], VAL_FONT, fg); cx += VAL_FONT_W;
    LCD_DrawChar_NoBg(cx, y, '0' + digits[3], VAL_FONT, fg); cx += VAL_FONT_W;
    LCD_DrawChar_NoBg(cx, y, '0' + digits[4], VAL_FONT, fg);
}

static void ApplyDigitDelta(float *val, float max_val, uint8_t d_idx, int16_t delta) {
    if (delta == 0) return;
    int32_t max_milli = (int32_t)(max_val * 1000.0f + 0.5f);
    int32_t milli = (int32_t)(*val * 1000.0f + 0.5f);
    if (milli < 0) milli = 0;
    if (milli > max_milli) milli = max_milli;

    int32_t mult = digit_multipliers[d_idx];
    int8_t old_d = (int8_t)((milli / mult) % 10);
    int8_t step = (delta > 0) ? 1 : -1;
    int new_d = old_d + step;
    if (new_d > 9) new_d = 0;
    if (new_d < 0) new_d = 9;

    int32_t new_milli = milli + (new_d - old_d) * mult;
    if (new_milli < 0 || new_milli > max_milli) return; /* 拒绝导致超限的修改 */
    *val = (float)new_milli / 1000.0f;
}

/* ==========================================================================
 * 页面生命周期回调
 * ========================================================================== */

static void Page_Output_Init(void) {
    edit_mode = OUT_MODE_IDLE;
    digit_idx = 1;
}

static void Page_Output_Enter(void) {
    edit_mode = OUT_MODE_IDLE;
    digit_idx = 1;
}

static void Page_Output_Exit(void) {
    edit_mode = OUT_MODE_IDLE;
    digit_idx = 1;
}

static void Page_Output_Tick(int16_t delta) {
    if (edit_mode == OUT_MODE_IDLE || delta == 0) return;

    if (edit_mode == OUT_MODE_EDIT_V) {
        ApplyDigitDelta(&powerState.target_v, V_MAX, digit_idx, delta);
    } else if (edit_mode == OUT_MODE_EDIT_I) {
        ApplyDigitDelta(&powerState.target_i, I_MAX, digit_idx, delta);
    }
}

static void Page_Output_OnButton(uint8_t btn_id, ButtonEvent_t event) {
    if (event != BTN_EVENT_SINGLE_CLICK) return;

    if (btn_id == 0) {
        /* 编码器按键：切换编辑项，默认回到个位 */
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
        digit_idx = 1;
    } else if (btn_id == 1) {
        /* Key_Left：光标左移 */
        if (edit_mode != OUT_MODE_IDLE && digit_idx > 0) {
            digit_idx--;
        }
    } else if (btn_id == 3) {
        /* Key_Right：光标右移 */
        if (edit_mode != OUT_MODE_IDLE && digit_idx < (DIGIT_COUNT - 1)) {
            digit_idx++;
        }
    }
}

static void Page_Output_Draw(void) {
    uint16_t bg = ST7789_COLOR_BLACK;
    uint16_t fg = ST7789_COLOR_WHITE;
    uint16_t box_color = ST7789_COLOR_YELLOW;

    static const uint16_t val_x    = 80;
    static const uint16_t val_y_v  = 55;
    static const uint16_t val_y_i  = 135;

    LCD_DrawString(10, 60,  "Vset:", LABEL_FONT, fg, bg);
    LCD_DrawString(220, 60, "V",     LABEL_FONT, fg, bg);
    DrawValueFixed(val_x, val_y_v, powerState.target_v,
                   (edit_mode == OUT_MODE_EDIT_V) ? digit_idx : 0xFF,
                   fg, bg, box_color);

    LCD_DrawString(10, 140,  "Iset:", LABEL_FONT, fg, bg);
    LCD_DrawString(220, 140, "A",     LABEL_FONT, fg, bg);
    DrawValueFixed(val_x, val_y_i, powerState.target_i,
                   (edit_mode == OUT_MODE_EDIT_I) ? digit_idx : 0xFF,
                   fg, bg, box_color);
}

/* ==========================================================================
 * 页面表
 * ========================================================================== */

UI_Page_t ui_page_output = {
    .name = "Output",
    .Init = Page_Output_Init,
    .Enter = Page_Output_Enter,
    .Exit = Page_Output_Exit,
    .Tick = Page_Output_Tick,
    .OnButton = Page_Output_OnButton,
    .Draw = Page_Output_Draw,
};
