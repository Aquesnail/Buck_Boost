#include "ST7789.h"
#include "main.h"
#include "spi.h"
#include "gpio.h"

// ==========================================
// 1. 内部缓冲区定义 (10行缓存)
// ==========================================
#define LCD_BUF_SIZE (240 * 10)
static uint16_t lcd_buffer[LCD_BUF_SIZE];

// ==========================================
// 2. 硬件底层回调实现
// ==========================================
static void HW_Write_CS(uint8_t state) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, (GPIO_PinState)state);
}

static void HW_Write_DC(uint8_t state) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, (GPIO_PinState)state);
}

static void HW_Write_RST(uint8_t state) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, (GPIO_PinState)state);
}

static void HW_Delay_ms(uint32_t ms) {
    HAL_Delay(ms);
}

static void HW_SPI_Send(uint8_t *data, uint16_t len) {
    // 使用阻塞式传输
    HAL_SPI_Transmit(&hspi2, data, len, 500);
}

static uint8_t HW_SPI_Send_DMA(uint8_t *data, uint16_t len) {
    // 检查 HAL 库的 SPI 状态，如果被占用则返回忙碌
    if (hspi2.State != HAL_SPI_STATE_READY) {
        return 1; 
    }
    
    // 启动 DMA 传输
    if (HAL_SPI_Transmit_DMA(&hspi2, data, len) == HAL_OK) {
        return 0; // 成功启动
    }
    
    return 1; // 启动失败
}

// ==========================================
// 3. 实例化驱动句柄
// ==========================================
ST7789_Handler_t st7789_main = {
    .width = 240,
    .height = 320,
    .x_offset = 0,
    .y_offset = 0,
    .rotation = ST7789_ROTATION_0,

    .Write_CS = HW_Write_CS,
    .Write_DC = HW_Write_DC,
    .Write_RST = HW_Write_RST,
    .Delay_ms = HW_Delay_ms,
    .SPI_Send = HW_SPI_Send,
    
    .SPI_Send_DMA = HW_SPI_Send_DMA, // <--- 修改点：挂载 DMA 发送函数

    .disp_buffer = lcd_buffer,
    .buffer_size = LCD_BUF_SIZE,
    .is_busy = 0
};

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    // 确保是控制屏幕的 SPI 发出的中断
    if (hspi->Instance == hspi2.Instance) { 
        // 调用底层框架准备好的 DMA 完成处理函数
        ST7789_DMA_TransferComplete_Callback(&st7789_main);
    }
}
// ==========================================
// 4. Port 层包装函数 (应用层直接调用这些)
// ==========================================

/**
 * @brief 初始化屏幕 (从 ST7789.c 转移并包装)
 */
void LCD_Init(void) {
    // 直接调用底层驱动的初始化逻辑
    ST7789_Init(&st7789_main);
}

/**
 * @brief 全屏涂满某个颜色 (带缓冲区中转)
 */
void LCD_Clear(uint16_t color) {
    ST7789_FillRect(&st7789_main, 0, 0, st7789_main.width, st7789_main.height, color);
}

/**
 * @brief 画点 (虽然不经过缓冲区，但在 Port 层统一入口)
 */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color) {
    // === 新增：等待之前的 DMA 传输完成 ===
    while (st7789_main.is_busy); 

    uint16_t color_swapped = (color << 8) | (color >> 8);
    ST7789_SetAddressWindow(&st7789_main, x, y, x, y);
    
    st7789_main.Write_DC(1);
    st7789_main.Write_CS(0);
    st7789_main.SPI_Send((uint8_t*)&color_swapped, 2);
    st7789_main.Write_CS(1);
}

/**
 * @brief 填充矩形 (带缓冲区中转)
 */
void LCD_Fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    ST7789_FillRect(&st7789_main, x, y, w, h, color);
}

/**
 * @brief 绘制图片 (带缓冲区中转)
 */
void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *data) {
    ST7789_DrawImage(&st7789_main, x, y, w, h, data);
}