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

static void DrawCharToBuf(uint16_t *buf, uint16_t buf_w,
                          uint16_t x, uint16_t y, char ch,
                          const ASCIIFont *font, uint16_t fg, uint16_t bg) {
    if (ch < ' ' || ch > '~') return;
    uint16_t char_index = ch - ' ';
    uint8_t bytes_per_row = (font->w + 7) / 8;
    uint16_t bytes_per_char = bytes_per_row * font->h;
    const uint8_t *data = &font->chars[char_index * bytes_per_char];

    for (uint8_t row = 0; row < font->h; row++) {
        for (uint8_t col = 0; col < font->w; col++) {
            uint8_t byte_idx = row * bytes_per_row + (col / 8);
            uint8_t bit_idx  = 7 - (col % 8);
            uint16_t color = (data[byte_idx] & (1 << bit_idx)) ? fg : bg;
            buf[(y + row) * buf_w + (x + col)] = color;
        }
    }
}

static void DrawRoundRectToBuf(uint16_t *buf, uint16_t buf_w,
                               uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                               uint16_t r, uint16_t color) {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (w == 0 || h == 0) return;

    if (w > 2 * r) {
        uint16_t line_w = w - 2 * r;
        for (uint16_t i = 0; i < line_w; i++) {
            buf[(y)         * buf_w + (x + r + i)] = color;
            buf[(y + h - 1) * buf_w + (x + r + i)] = color;
        }
    }
    if (h > 2 * r) {
        uint16_t line_h = h - 2 * r;
        for (uint16_t i = 0; i < line_h; i++) {
            buf[(y + r + i) * buf_w + (x)]         = color;
            buf[(y + r + i) * buf_w + (x + w - 1)] = color;
        }
    }
    for (uint16_t i = 0; i < r; i++) {
        uint16_t edge = r - 1 - i;
        uint16_t y_top    = y + i;
        uint16_t y_bottom = y + h - 1 - i;
        uint16_t x_left   = x + edge;
        uint16_t x_right  = x + w - 1 - edge;

        buf[y_top    * buf_w + x_left]  = color;
        if (x_right != x_left) buf[y_top    * buf_w + x_right] = color;
        if (y_bottom != y_top) {
            buf[y_bottom * buf_w + x_left]  = color;
            if (x_right != x_left) buf[y_bottom * buf_w + x_right] = color;
        }
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

    uint16_t pad = 1;
    uint16_t r   = 2;
    uint16_t cell_w = VAL_FONT_W + pad * 2; /* 14 */
    uint16_t cell_h = VAL_FONT_H + pad * 2; /* 26 */
    uint16_t dot_w  = VAL_FONT_W;           /* 12 */

    /* 布局: [cell][cell][dot][cell][cell][cell] */
    uint16_t total_w = cell_w * 2 + dot_w + cell_w * 3; /* 82 */
    uint16_t total_h = cell_h;                          /* 26 */

    static uint16_t val_buf[84 * 26];
    uint32_t pixels = total_w * total_h;
    for (uint32_t i = 0; i < pixels; i++) val_buf[i] = bg;

    /* 1. 画选中框 (仅边框) */
    for (int d = 0; d < DIGIT_COUNT; d++) {
        if (sel_digit == d) {
            uint16_t ox = (d < 2) ? (d * cell_w) : (2 * cell_w + dot_w + (d - 2) * cell_w);
            DrawRoundRectToBuf(val_buf, total_w, ox, 0, cell_w, cell_h, r, box_color);
        }
    }

    /* 2. 画字符到 buffer */
    uint16_t cx = pad;
    DrawCharToBuf(val_buf, total_w, cx, pad, '0' + digits[0], VAL_FONT, fg, bg); cx += VAL_FONT_W + pad * 2;
    DrawCharToBuf(val_buf, total_w, cx, pad, '0' + digits[1], VAL_FONT, fg, bg); cx += VAL_FONT_W + pad * 2;
    DrawCharToBuf(val_buf, total_w, cx, pad, '.',            VAL_FONT, fg, bg); cx += VAL_FONT_W;
    DrawCharToBuf(val_buf, total_w, cx, pad, '0' + digits[2], VAL_FONT, fg, bg); cx += VAL_FONT_W + pad * 2;
    DrawCharToBuf(val_buf, total_w, cx, pad, '0' + digits[3], VAL_FONT, fg, bg); cx += VAL_FONT_W + pad * 2;
    DrawCharToBuf(val_buf, total_w, cx, pad, '0' + digits[4], VAL_FONT, fg, bg);

    /* 3. 一次性 DMA 推送整串数值 */
    LCD_DrawImage(x - pad, y - pad, total_w, total_h, val_buf);
}

static void ApplyDigitDelta(float *val, float max_val, uint8_t d_idx, int16_t delta) {
    if (delta == 0) return;
    int32_t max_milli = (int32_t)(max_val * 1000.0f + 0.5f);
    int32_t milli = (int32_t)(*val * 1000.0f + 0.5f);
    if (milli < 0) milli = 0;
    if (milli > max_milli) milli = max_milli;

    /* 将当前值分解为每位数字 */
    int digits[DIGIT_COUNT];
    int32_t temp_milli = milli;
    for (int i = 0; i < DIGIT_COUNT; i++) {
        digits[i] = (temp_milli / digit_multipliers[i]) % 10;
        if (temp_milli >= digit_multipliers[i]) {
            temp_milli = temp_milli % digit_multipliers[i];
        }
    }

    /* 从当前修改位开始进位/借位传播 */
    int step = (delta > 0) ? 1 : -1;
    int carry = step;
    for (int i = d_idx; i >= 0 && carry != 0; i--) {
        int new_d = digits[i] + carry;
        if (new_d > 9) {
            digits[i] = new_d - 10;
            carry = 1;
        } else if (new_d < 0) {
            digits[i] = new_d + 10;
            carry = -1;
        } else {
            digits[i] = new_d;
            carry = 0;
        }
    }

    /* 检查结果是否在有效范围内 */
    int32_t new_milli = 0;
    for (int i = 0; i < DIGIT_COUNT; i++) {
        new_milli += digits[i] * digit_multipliers[i];
    }

    if (new_milli < 0 || new_milli > max_milli) return;
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

    char buf[64];

    /* 实时测量值 — 显示在页面上方 */
    sprintf(buf, "Vin:%.2fV Vout:%.2fV Iin:%.3fA",
            powerMeas.vin, powerMeas.vout, powerMeas.iin);
    LCD_DrawString(5, 2, buf, LABEL_FONT, fg, bg);

    sprintf(buf, "Iout:%.3fA IL:%.3fA",
            powerMeas.iout, powerMeas.inductor_i);
    LCD_DrawString(5, 20, buf, LABEL_FONT, fg, bg);

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
