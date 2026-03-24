#ifndef __ST7789_H
#define __ST7789_H

#include <stdint.h>

// 旋转方向定义
typedef enum {
    ST7789_ROTATION_0 = 0,
    ST7789_ROTATION_90,
    ST7789_ROTATION_180,
    ST7789_ROTATION_270
} ST7789_Rotation_t;

// 驱动句柄结构体
typedef struct ST7789_Handler {
    // 1. 基础配置参数
    uint16_t width;        // 屏幕物理宽度
    uint16_t height;       // 屏幕物理高度
    uint16_t x_offset;     // X轴偏移 (针对非240x320屏幕)
    uint16_t y_offset;     // Y轴偏移
    ST7789_Rotation_t rotation;

    // 2. 硬件接口函数指针
    void (*Write_CS)(uint8_t state);
    void (*Write_DC)(uint8_t state);
    void (*Write_RST)(uint8_t state);
    void (*Delay_ms)(uint32_t ms);
    
    // 阻塞式发送函数 (必选)
    void (*SPI_Send)(uint8_t *data, uint16_t len);
    
    // DMA发送函数 (可选，若为NULL则自动使用SPI_Send)
    // 返回值: 0-成功启动, 1-忙碌/失败
    uint8_t (*SPI_Send_DMA)(uint8_t *data, uint16_t len);

    // 3. DMA 状态管理
    volatile uint8_t is_busy; 
    
    // 4. 外部缓冲区指针 (由Port层分配)
    uint16_t *disp_buffer;
    uint32_t buffer_size; // 单位：像素数(uint16_t)
} ST7789_Handler_t;

// API 声明
void ST7789_Init(ST7789_Handler_t *handler);
void ST7789_SetAddressWindow(ST7789_Handler_t *handler, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void ST7789_FillRect(ST7789_Handler_t *handler, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7789_DrawImage(ST7789_Handler_t *handler, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *data);
void ST7789_SetRotation(ST7789_Handler_t *handler, ST7789_Rotation_t rotation);

// DMA 完成回调函数 (需在硬件中断中调用)
void ST7789_DMA_TransferComplete_Callback(ST7789_Handler_t *handler);

// ============================================================
// 常用颜色定义 (RGB565格式)
// ============================================================
#define ST7789_COLOR_BLACK       0x0000
#define ST7789_COLOR_WHITE       0xFFFF
#define ST7789_COLOR_RED         0xF800
#define ST7789_COLOR_GREEN       0x07E0
#define ST7789_COLOR_BLUE        0x001F
#define ST7789_COLOR_YELLOW      0xFFE0
#define ST7789_COLOR_CYAN        0x07FF
#define ST7789_COLOR_MAGENTA     0xF81F
#define ST7789_COLOR_ORANGE      0xFA20
#define ST7789_COLOR_PURPLE      0x781F
#define ST7789_COLOR_PINK        0xF8C8
#define ST7789_COLOR_GRAY        0x8410
#define ST7789_COLOR_LIGHT_GRAY  0xC618
#define ST7789_COLOR_DARK_GRAY   0x4208

// ============================================================
// 常用辅助函数
// ============================================================

// 获取驱动句柄的外部声明 (在ST7789.c中定义)
extern ST7789_Handler_t st7789_main;

#endif