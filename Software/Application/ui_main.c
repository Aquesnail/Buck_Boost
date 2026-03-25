#include "ui_main.h"
#include "ui_interaction.h"
#include "ST7789_SPI_Port.h"
#include "power_ui_api.h"
#include "font.h"
#include "stdio.h" // 用于 sprintf

// ====== 主题颜色定义 (RGB565) ======
#define COLOR_BG        0x0000  // 纯黑背景
#define COLOR_TEXT      0xFFFF  // 纯白文字
#define COLOR_GRAY      0x8410  // 灰色（用于分割线和次要标签）
#define COLOR_V_OUT     0x07FF  // 青色（输出电压高亮）
#define COLOR_I_OUT     0xFFE0  // 黄色（输出电流高亮）
#define COLOR_ON        0x07E0  // 绿色（开启状态）
#define COLOR_OFF       0xF800  // 红色（关闭/警告状态）
#define COLOR_CYAN      0xF800

// ====== 布局系数量 ======
#define ZONE_A_Y        4       // 顶部状态栏 Y坐标
#define LINE_TOP_Y      28      // 顶部分割线 Y坐标
#define LINE_MID_X      150     // 中间垂直分割线 X坐标
#define LINE_BOT_Y      185     // 底部分割线 Y坐标

void UI_DrawMainScreen_Static(void) {
    // 1. 全屏刷黑
    LCD_Clear(COLOR_BG);

    // 2. 绘制框架分割线 (保持 320x240 布局)
    LCD_Fill(0, LINE_TOP_Y, 320, 2, COLOR_GRAY);               // Top line
    LCD_Fill(LINE_MID_X, LINE_TOP_Y, 2, 157, COLOR_GRAY);      // Vertical divider
    LCD_Fill(0, LINE_BOT_Y, 320, 2, COLOR_GRAY);               // Bottom line

    // 3. Zone A: System Status Labels
    LCD_DrawString(5, ZONE_A_Y + 4, "STATUS:", &afont12x6, COLOR_TEXT, COLOR_BG);
    LCD_DrawString(120, ZONE_A_Y + 4, "MODE:", &afont12x6, COLOR_TEXT, COLOR_BG);

    // 4. Zone C: Settings (Left Side)
    LCD_DrawString(10, 45, "SET VOLTAGE", &afont12x6, COLOR_GRAY, COLOR_BG);
    LCD_DrawString(10, 95, "SET CURR MAX", &afont12x6, COLOR_GRAY, COLOR_BG);
    LCD_DrawString(10, 145, "INPUT MONITOR", &afont12x6, COLOR_GRAY, COLOR_BG);

    // 5. Zone B: Measurements (Right Side)
    // 标签稍微缩小，把视觉重点留给数值
    LCD_DrawString(165, 45, "V-OUTPUT", &afont12x6, COLOR_V_OUT, COLOR_BG);
    LCD_DrawString(165, 110, "I-OUTPUT", &afont12x6, COLOR_I_OUT, COLOR_BG);

    // 6. Zone D: Inductor Status
    LCD_DrawString(5, 205, "L-CURR (AVG):", &afont12x6, COLOR_GRAY, COLOR_BG);
}

void UI_UpdateMainScreen_Dynamic(void) {
    char str_buf[20];
    PowerMeas_t meas;
    PowerState_t state;

    PowerAPI_GetMeasurements(&meas);
    PowerAPI_GetState(&state);

    // --- Zone A: Status Indicators ---
    if (state.out_enable) {
        LCD_DrawString(55, ZONE_A_Y + 2, " ACTIVE ", &afont16x8, COLOR_BG, COLOR_ON); 
    } else {
        LCD_DrawString(55, ZONE_A_Y + 2, " DISABLE", &afont16x8, COLOR_TEXT, COLOR_OFF);
    }

    const char* mode_str = (state.mode == SYS_RUN_CV) ? "CV" : 
                           (state.mode == SYS_RUN_CC) ? "CC" : "IDLE";
    uint16_t mode_color = (state.mode == SYS_RUN_CV) ? COLOR_V_OUT : 
                          (state.mode == SYS_RUN_CC) ? COLOR_I_OUT : COLOR_GRAY;
    LCD_DrawString(160, ZONE_A_Y + 2, mode_str, &afont16x8, mode_color, COLOR_BG);

    // --- Zone C: Target Settings ---
    sprintf(str_buf, "%5.2f V", state.V_set);
    LCD_DrawString(15, 65, str_buf, &afont16x8, COLOR_TEXT, COLOR_BG);
    
    sprintf(str_buf, "%5.2f A", state.I_set);
    LCD_DrawString(15, 115, str_buf, &afont16x8, COLOR_TEXT, COLOR_BG);

    sprintf(str_buf, "Vin:%5.2fV", meas.Vin);
    LCD_DrawString(15, 165, str_buf, &afont12x6, COLOR_GRAY, COLOR_BG);

    // --- Zone B: Big Numbers (Real-time) ---
    // 电压使用大字号 24x12
    sprintf(str_buf, "%5.2f V", meas.Vout);
    LCD_DrawString(165, 65, str_buf, &afont24x12, COLOR_V_OUT, COLOR_BG);

    sprintf(str_buf, "%5.2f A", meas.Iout);
    LCD_DrawString(165, 130, str_buf, &afont24x12, COLOR_I_OUT, COLOR_BG);

    sprintf(str_buf, "P: %5.2f W", meas.Pout);
    LCD_DrawString(165, 165, str_buf, &afont12x6, COLOR_TEXT, COLOR_BG);

    // --- Zone D: Bottom Bar ---
    sprintf(str_buf, "%4.2f A", meas.IL_avg);
    LCD_DrawString(95, 205, str_buf, &afont12x6, COLOR_TEXT, COLOR_BG);

    // Bar 逻辑保持不变
    uint16_t bar_w = (uint16_t)((meas.IL_avg / 10.0f) * 140.0f);
    if (bar_w > 140) bar_w = 140;
    LCD_Fill(170, 206, bar_w, 12, (meas.IL_avg > 8.0f) ? COLOR_OFF : 0x07FF);
    if (bar_w < 140) LCD_Fill(170 + bar_w, 206, 140 - bar_w, 12, COLOR_BG);
}