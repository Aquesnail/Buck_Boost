#include "ST7789_SPI_Port.h"
#include "ST7789.h"
#include "main.h"
#include "spi.h"
#include "gpio.h"

// ==========================================
// 1. 内部缓冲区定义 (10行缓存)
// ==========================================
#define LCD_BUF_SIZE (230 * 24)
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
    .width = 320,
    .height = 240,
    .x_offset = 0,
    .y_offset = 0,
    .rotation = ST7789_ROTATION_90,

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

void SPI_Tx_DMA_Callback(void) {
    // 确保是控制屏幕的 SPI 发出的中断
        // 调用底层框架准备好的 DMA 完成处理函数
        ST7789_DMA_TransferComplete_Callback(&st7789_main);
    
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
 * @brief 填充圆角矩形 (小半径近似，r 建议 2~4)
 */
void LCD_FillRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t color) {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    /* 中间主体 */
    LCD_Fill(x + r, y, w - 2 * r, h, color);
    /* 左右两竖条 */
    LCD_Fill(x, y + r, r, h - 2 * r, color);
    LCD_Fill(x + w - r, y + r, r, h - 2 * r, color);

    /* 四个圆角 */
    for (uint16_t i = 0; i < r; i++) {
        uint16_t seg_w = i + 1;
        uint16_t seg_x_left  = x + r - seg_w;
        uint16_t seg_x_right = x + w - r;
        uint16_t y_top       = y + i;
        uint16_t y_bottom    = y + h - 1 - i;
        LCD_Fill(seg_x_left,  y_top,    seg_w, 1, color);
        LCD_Fill(seg_x_right, y_top,    seg_w, 1, color);
        LCD_Fill(seg_x_left,  y_bottom, seg_w, 1, color);
        LCD_Fill(seg_x_right, y_bottom, seg_w, 1, color);
    }
}

/**
 * @brief 绘制空心圆角矩形 (仅画 1px 边框线)
 */
void LCD_DrawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t color) {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (w == 0 || h == 0) return;

    /* 上下横线 (中间主体，不含圆角尖) */
    if (w > 2 * r) {
        uint16_t line_w = w - 2 * r;
        LCD_Fill(x + r, y,          line_w, 1, color); /* 上边 */
        LCD_Fill(x + r, y + h - 1,  line_w, 1, color); /* 下边 */
    }

    /* 左右竖线 (中间主体，不含圆角尖) */
    if (h > 2 * r) {
        uint16_t line_h = h - 2 * r;
        LCD_Fill(x,         y + r, 1, line_h, color); /* 左边 */
        LCD_Fill(x + w - 1, y + r, 1, line_h, color); /* 右边 */
    }

    /* 四个圆角边框点 */
    for (uint16_t i = 0; i < r; i++) {
        uint16_t edge = r - 1 - i; /* 从外向内递减 */
        uint16_t y_top    = y + i;
        uint16_t y_bottom = y + h - 1 - i;
        uint16_t x_left   = x + edge;
        uint16_t x_right  = x + w - 1 - edge;

        /* 当 w/h 很小时避免重复画点 */
        LCD_DrawPoint(x_left,  y_top,    color); /* 左上 */
        if (x_right != x_left) {
            LCD_DrawPoint(x_right, y_top,    color); /* 右上 */
        }
        if (y_bottom != y_top) {
            LCD_DrawPoint(x_left,  y_bottom, color); /* 左下 */
            if (x_right != x_left) {
                LCD_DrawPoint(x_right, y_bottom, color); /* 右下 */
            }
        }
    }
}

/**
 * @brief 绘制图片 (带缓冲区中转)
 */
void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *data) {
    ST7789_DrawImage(&st7789_main, x, y, w, h, data);
}

// ==========================================
// 5. 文本渲染层 (带 DMA 局部缓冲加速)
// ==========================================

// 定义最大字符缓冲，24x24 字符需要 576 个 uint16_t (1152 Bytes)
// 放在栈里或者作为静态变量，这里用局部变量方便多线程重入
#define MAX_CHAR_PIXELS 576 

/**
 * @brief 绘制单个 ASCII 字符 (适配"逐行式"字模)
 */
void LCD_DrawChar(uint16_t x, uint16_t y, char ch, const ASCIIFont *font, uint16_t fg_color, uint16_t bg_color) {
    if (ch < ' ' || ch > '~') return; // 非法字符拦截

    uint16_t char_index = ch - ' ';
    // 计算每行需要几个字节 (比如宽 12px，每行就需要 2 个字节)
    uint8_t bytes_per_row = (font->w + 7) / 8; 
    uint16_t bytes_per_char = bytes_per_row * font->h;
    const uint8_t *data = &font->chars[char_index * bytes_per_char];

    uint16_t char_buf[MAX_CHAR_PIXELS];
    if (font->w * font->h > MAX_CHAR_PIXELS) return; // 防止缓存越界

    uint16_t pixel_idx = 0; // 线性缓冲区指针，不再需要乘法计算二维坐标

    // 逐行式解析：外层循环是行，内层循环是列
    for (uint8_t row = 0; row < font->h; row++) {
        for (uint8_t col = 0; col < font->w; col++) {
            // 找到当前像素对应在哪个字节
            uint8_t byte_idx = row * bytes_per_row + (col / 8);
            // 找到当前像素是该字节的第几位 (按照"高位在前"，最左边是 bit 7)
            uint8_t bit_idx = 7 - (col % 8); 

            // 判断字模当前 bit，填入颜色
            if (data[byte_idx] & (1 << bit_idx)) {
                char_buf[pixel_idx++] = fg_color;
            } else {
                char_buf[pixel_idx++] = bg_color;
            }
        }
    }
    
    // 一次性 DMA 推送
    LCD_DrawImage(x, y, font->w, font->h, char_buf);
}

/**
 * @brief 绘制单个 ASCII 字符 (无背景，只绘制前景像素)
 */
void LCD_DrawChar_NoBg(uint16_t x, uint16_t y, char ch, const ASCIIFont *font, uint16_t fg_color) {
    if (ch < ' ' || ch > '~') return; // 非法字符拦截

    uint16_t char_index = ch - ' ';
    uint8_t bytes_per_row = (font->w + 7) / 8;
    uint16_t bytes_per_char = bytes_per_row * font->h;
    const uint8_t *data = &font->chars[char_index * bytes_per_char];

    for (uint8_t row = 0; row < font->h; row++) {
        for (uint8_t col = 0; col < font->w; col++) {
            uint8_t byte_idx = row * bytes_per_row + (col / 8);
            uint8_t bit_idx = 7 - (col % 8);
            if (data[byte_idx] & (1 << bit_idx)) {
                LCD_DrawPoint(x + col, y + row, fg_color);
            }
        }
    }
}

/**
 * @brief 绘制 ASCII 字符串
 */
void LCD_DrawString(uint16_t x, uint16_t y, const char *str, const ASCIIFont *font, uint16_t fg_color, uint16_t bg_color) {
    uint16_t curr_x = x;
    uint16_t curr_y = y;

    while (*str != '\0') {
        if (*str == '\n') { // 支持简单的换行符
            curr_x = x;
            curr_y += font->h;
        } else {
            LCD_DrawChar(curr_x, curr_y, *str, font, fg_color, bg_color);
            curr_x += font->w;
        }
        str++;
    }
}

/**
 * @brief 内部调用：绘制单个中文 UTF-8 字符 (依赖 font.h 中的 Font 结构体)
 */
static void LCD_DrawChineseChar(uint16_t x, uint16_t y, const uint8_t *utf8_code, const Font *font, uint16_t fg_color, uint16_t bg_color) {
    uint8_t pages = (font->h + 7) / 8;
    uint16_t bytes_per_char = font->w * pages;
    uint16_t block_size = 4 + bytes_per_char; // 4字节UTF8头 + 字模数据

    const uint8_t *data = NULL;
    // 遍历字库查找匹配的 UTF-8 编码
    for (uint8_t i = 0; i < font->len; i++) {
        const uint8_t *entry = &font->chars[i * block_size];
        if (entry[0] == utf8_code[0] && entry[1] == utf8_code[1] && entry[2] == utf8_code[2]) {
            data = entry + 4; // 找到字模起点
            break;
        }
    }

    if (data == NULL) return; // 没找到该汉字

    uint16_t char_buf[MAX_CHAR_PIXELS];
    if (font->w * font->h > MAX_CHAR_PIXELS) return;

    for (uint8_t p = 0; p < pages; p++) {
        for (uint8_t col = 0; col < font->w; col++) {
            uint8_t byte = data[p * font->w + col];
            for (uint8_t bit = 0; bit < 8; bit++) {
                uint8_t row = p * 8 + bit;
                if (row >= font->h) break;

                uint16_t color = (byte & (1 << bit)) ? fg_color : bg_color;
                char_buf[row * font->w + col] = color;
            }
        }
    }
    LCD_DrawImage(x, y, font->w, font->h, char_buf);
}

/**
 * @brief 智能绘制中英文混合文本
 */
void LCD_DrawText(uint16_t x, uint16_t y, const char *str, const Font *font, uint16_t fg_color, uint16_t bg_color) {
    uint16_t curr_x = x;
    uint16_t curr_y = y;
    const uint8_t *ptr = (const uint8_t *)str;

    while (*ptr != '\0') {
        if (*ptr == '\n') {
            curr_x = x;
            curr_y += font->h;
            ptr++;
        } else if (*ptr < 0x80) { 
            // ASCII 处理 (调用挂载的英文字体)
            LCD_DrawChar(curr_x, curr_y, *ptr, font->ascii, fg_color, bg_color);
            curr_x += font->ascii->w;
            ptr++;
        } else { 
            // 默认按 3 字节 UTF-8 处理
            LCD_DrawChineseChar(curr_x, curr_y, ptr, font, fg_color, bg_color);
            curr_x += font->w;
            ptr += 3;
        }
    }
}