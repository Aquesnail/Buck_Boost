#ifndef __ST7789_SPI_PORT_H
#define __ST7789_SPI_PORT_H

#include "stdint.h"

// 对外接口
void LCD_Init(void);
void LCD_Clear(uint16_t color);
void LCD_Fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *data);

#endif