#ifndef __ST7789_SPI_PORT_H
#define __ST7789_SPI_PORT_H

#include "stdint.h"
#include "font.h"
// 对外接口
void LCD_Init(void);
void LCD_Clear(uint16_t color);
void LCD_Fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *data);

void LCD_DrawChar(uint16_t x, uint16_t y, char ch, const ASCIIFont *font, uint16_t fg_color, uint16_t bg_color);
void LCD_DrawString(uint16_t x, uint16_t y, const char *str, const ASCIIFont *font, uint16_t fg_color, uint16_t bg_color);

// === 新增：中英混合输出接口 (支持 UTF-8) ===
void LCD_DrawText(uint16_t x, uint16_t y, const char *str, const Font *font, uint16_t fg_color, uint16_t bg_color);

extern void SPI_Tx_DMA_Callback(void);
#endif