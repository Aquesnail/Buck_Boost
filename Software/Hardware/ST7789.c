#include "ST7789.h"
#include "ST7789_Reg.h" // 包含之前定义的宏

#include "stddef.h"


// 内部私有函数：发送命令
static void ST7789_WriteCommand(ST7789_Handler_t *h, uint8_t cmd) {
    // === 关键修复：等待之前的 DMA 彻底完成 ===
    if (h->SPI_Send_DMA != NULL) {
        while(h->is_busy); 
    }

    h->Write_DC(0); // 命令模式
    h->Write_CS(0);
    h->SPI_Send(&cmd, 1); // 发送命令必然是短数据，用阻塞即可
    h->Write_CS(1);
}

// 内部私有函数：发送数据
static void ST7789_WriteData(ST7789_Handler_t *h, uint8_t *data, uint16_t len) {
    // === 关键修复：等待之前的 DMA 彻底完成 ===
    if (h->SPI_Send_DMA != NULL) {
        while(h->is_busy); 
    }

    h->Write_DC(1); // 数据模式
    h->Write_CS(0);
    h->SPI_Send(data, len); // 发送参数必然是短数据，用阻塞即可
    h->Write_CS(1);
}

// 核心发送函数：支持DMA与阻塞切换
static void ST7789_TransmitData(ST7789_Handler_t *h, uint8_t *data, uint32_t len) {
     if (h->SPI_Send_DMA != NULL) {
        while(h->is_busy);  //这个等待非常非常重要
    }

    h->Write_DC(1); // 切换到像素数据模式
    h->Write_CS(0); // 拉低 CS 选中屏幕
    
    if (h->SPI_Send_DMA != NULL) {
        h->is_busy = 1; // 挂起忙碌标志
        if (h->SPI_Send_DMA(data, (uint16_t)len) != 0) {
            // 如果 DMA 启动失败，降级为阻塞发送
            h->is_busy = 0;
            h->SPI_Send(data, (uint16_t)len);
            h->Write_CS(1);
        }
        // 注意：这里不要拉高 CS！交由 DMA 传输完成的中断去拉高。
    } else {
        // 无 DMA 支持时的纯阻塞模式
        h->SPI_Send(data, (uint16_t)len);
        h->Write_CS(1); 
    }
}

void ST7789_Init(ST7789_Handler_t *handler) {
    // 硬件复位
    if(handler->Write_RST) {
        handler->Write_RST(0);
        handler->Delay_ms(10);
        handler->Write_RST(1);
        handler->Delay_ms(120);
    }

    ST7789_WriteCommand(handler, ST7789_CMD_SWRESET);
    handler->Delay_ms(150);

    ST7789_WriteCommand(handler, ST7789_CMD_SLPOUT);
    handler->Delay_ms(120);

    // 默认颜色格式 16bit RGB565
    uint8_t colmod = ST7789_COLMOD_16BIT | (ST7789_COLMOD_16BIT << 4);
    ST7789_WriteCommand(handler, ST7789_CMD_COLMOD);
    ST7789_WriteData(handler, &colmod, 1);

    ST7789_SetRotation(handler, handler->rotation);

    ST7789_WriteCommand(handler, ST7789_CMD_INVON); // 通常IPS屏需要开启反显
    ST7789_WriteCommand(handler, ST7789_CMD_NORON);
    ST7789_WriteCommand(handler, ST7789_CMD_DISPON);
}

void ST7789_SetAddressWindow(ST7789_Handler_t *h, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    uint16_t x_start = x1 + h->x_offset;
    uint16_t x_end = x2 + h->x_offset;
    uint16_t y_start = y1 + h->y_offset;
    uint16_t y_end = y2 + h->y_offset;

    uint8_t data[4];
    
    ST7789_WriteCommand(h, ST7789_CMD_CASET);
    data[0] = x_start >> 8; data[1] = x_start & 0xFF;
    data[2] = x_end >> 8;   data[3] = x_end & 0xFF;
    ST7789_WriteData(h, data, 4);

    ST7789_WriteCommand(h, ST7789_CMD_RASET);
    data[0] = y_start >> 8; data[1] = y_start & 0xFF;
    data[2] = y_end >> 8;   data[3] = y_end & 0xFF;
    ST7789_WriteData(h, data, 4);

    ST7789_WriteCommand(h, ST7789_CMD_RAMWR);
}

void ST7789_FillRect(ST7789_Handler_t *h, uint16_t x, uint16_t y, uint16_t w, uint16_t h_rect, uint16_t color) {
    ST7789_SetAddressWindow(h, x, y, x + w - 1, y + h_rect - 1);
    
    uint32_t total_pixels = w * h_rect;
    uint16_t color_swapped = (color << 8) | (color >> 8); // 大端序转换

    // 如果没有缓冲区，只能逐点发送（慢）
    if (h->disp_buffer == NULL) {
        for (uint32_t i = 0; i < total_pixels; i++) {
            ST7789_WriteData(h, (uint8_t*)&color_swapped, 2);
        }
        return;
    }

    // 使用缓冲区批量发送
    uint32_t pixels_to_send = total_pixels;
    while(pixels_to_send > 0) {
        uint32_t chunk_size = (pixels_to_send > h->buffer_size) ? h->buffer_size : pixels_to_send;
        
        // 填充缓冲区
        for(uint32_t i = 0; i < chunk_size; i++) {
            h->disp_buffer[i] = color_swapped;
        }
        
        ST7789_TransmitData(h, (uint8_t*)h->disp_buffer, chunk_size * 2);
        pixels_to_send -= chunk_size;
    }
}

void ST7789_DMA_TransferComplete_Callback(ST7789_Handler_t *handler) {
    handler->is_busy = 0;
    handler->Write_CS(1); // 传输完成，拉高CS
}

/**
 * @brief 设置屏幕旋转方向
 * @param rotation: 0-3 分别代表 0, 90, 180, 270 度
 */
void ST7789_SetRotation(ST7789_Handler_t *handler, ST7789_Rotation_t rotation) {
    uint8_t madctl = 0;
    handler->rotation = rotation;

    /* * ST7789 的 MADCTL (0x36) 寄存器控制逻辑:
     * MY: 行地址顺序  MX: 列地址顺序  MV: 行列交换
     */
    switch (rotation) {
        case ST7789_ROTATION_0:
            madctl = ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB;
            // 如果是常见的 240x240 屏幕，Rotation 0 的偏移通常是 (0, 0)
            break;
        case ST7789_ROTATION_90:
            madctl = ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB;
            // 横屏模式，行列交换
            break;
        case ST7789_ROTATION_180:
            madctl = ST7789_MADCTL_RGB;
            // 针对 240x240，180度时可能需要 y_offset = 80 (针对240x320驱动器)
            break;
        case ST7789_ROTATION_270:
            madctl = ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB;
            break;
        default:
            return;
    }

    ST7789_WriteCommand(handler, ST7789_CMD_MADCTL);
    ST7789_WriteData(handler, &madctl, 1);

    // 根据旋转角度调整逻辑分辨率
    // 如果是 90 或 270 度，宽度和高度互换
    if (rotation == ST7789_ROTATION_90 || rotation == ST7789_ROTATION_270) {
        if (handler->width < handler->height) {
            uint16_t temp = handler->width;
            handler->width = handler->height;
            handler->height = temp;
        }
    } else {
        if (handler->width > handler->height) {
            uint16_t temp = handler->width;
            handler->width = handler->height;
            handler->height = temp;
        }
    }
}

/**
 * @brief 在指定区域绘制图片
 * @param data: 图片颜色数组 (RGB565格式)
 */
void ST7789_DrawImage(ST7789_Handler_t *handler, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *data) {
    // 1. 设置绘制窗口
    ST7789_SetAddressWindow(handler, x, y, x + w - 1, y + h - 1);

    uint32_t total_size = w * h;
    
    /*
     * 注意：ST7789 要求 16bit 颜色先传高 8 位 (MSB)。
     * 如果你的图片数组 data 已经是大端序，可以直接发送。
     * 如果是小端序（大部分 PC 转换工具默认导出是小端），
     * 且你有 disp_buffer，我们建议分块中转处理以支持 DMA。
     */
    
    if (handler->disp_buffer != NULL) {
        uint32_t pixels_processed = 0;
        while (pixels_processed < total_size) {
            uint32_t chunk_size = (total_size - pixels_processed > handler->buffer_size) ? 
                                   handler->buffer_size : (total_size - pixels_processed);
            
            // 字节序转换（如果需要）并拷贝到缓冲区
            for (uint32_t i = 0; i < chunk_size; i++) {
                uint16_t color = data[pixels_processed + i];
                handler->disp_buffer[i] = (color << 8) | (color >> 8);
            }
            
            // 发送这一块数据
            ST7789_TransmitData(handler, (uint8_t*)handler->disp_buffer, chunk_size * 2);
            pixels_processed += chunk_size;
        }
    } else {
        // 无缓冲区模式：阻塞式直接发送（要求 data 已经是大端序）
        handler->Write_DC(1);
        handler->Write_CS(0);
        handler->SPI_Send((uint8_t*)data, total_size * 2);
        handler->Write_CS(1);
    }
}