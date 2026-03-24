#ifndef _ST7789_REG_H
#define _ST7789_REG_H

/* 寄存器地址定义 */
#define ST7789_CMD_NOP          0x00 // 无操作
#define ST7789_CMD_SWRESET      0x01 // 软件复位
#define ST7789_CMD_RDDID        0x04 // 读取显示 ID (参数: Dummy, ID1, ID2, ID3)
#define ST7789_CMD_RDDST        0x09 // 读取显示状态
#define ST7789_CMD_RDDPM        0x0A // 读取显示电源模式
#define ST7789_CMD_RDDMADCTL    0x0B // 读取 MADCTL
#define ST7789_CMD_RDDCOLMOD    0x0C // 读取像素格式
#define ST7789_CMD_RDDIM        0x0D // 读取图像模式
#define ST7789_CMD_RDDSM        0x0E // 读取信号状态
#define ST7789_CMD_RDDSDR       0x0F // 读取自诊断结果
#define ST7789_CMD_SLPIN        0x10 // 进入睡眠模式
#define ST7789_CMD_SLPOUT       0x11 // 退出睡眠模式
#define ST7789_CMD_PTLON        0x12 // 局部显示模式开启
#define ST7789_CMD_NORON        0x13 // 普通显示模式开启 (局部模式关闭)
#define ST7789_CMD_INVOFF       0x20 // 显示反显关闭
#define ST7789_CMD_INVON        0x21 // 显示反显开启
#define ST7789_CMD_GAMSET       0x26 // 伽马设置
#define ST7789_CMD_DISPOFF      0x28 // 显示关闭
#define ST7789_CMD_DISPON       0x29 // 显示开启
#define ST7789_CMD_CASET        0x2A // 列地址设置
#define ST7789_CMD_RASET        0x2B // 行地址设置
#define ST7789_CMD_RAMWR        0x2C // 存储器写入
#define ST7789_CMD_RAMRD        0x2E // 存储器读取
#define ST7789_CMD_PTLAR        0x30 // 局部区域设置
#define ST7789_CMD_VSCRDEF      0x33 // 垂直滚动定义
#define ST7789_CMD_TEOFF        0x34 // 撕裂效果行关闭
#define ST7789_CMD_TEON         0x35 // 撕裂效果行开启
#define ST7789_CMD_MADCTL       0x36 // 存储数据访问控制
#define ST7789_CMD_VSCSAD       0x37 // 垂直滚动起始地址
#define ST7789_CMD_IDMOFF       0x38 // 空闲模式关闭
#define ST7789_CMD_IDMON        0x39 // 空闲模式开启
#define ST7789_CMD_COLMOD       0x3A // 接口像素格式
#define ST7789_CMD_RAMWRC       0x3C // 存储器持续写入
#define ST7789_CMD_RAMRDC       0x3E // 存储器持续读取
#define ST7789_CMD_TESCAN       0x44 // 设置撕裂扫描线
#define ST7789_CMD_RDTESCAN     0x45 // 读取撕裂扫描线
#define ST7789_CMD_WRDISBV      0x51 // 写入显示亮度
#define ST7789_CMD_RDDISBV      0x52 // 读取显示亮度
#define ST7789_CMD_WRCTRLD      0x53 // 写入控制显示
#define ST7789_CMD_RDCTRLD      0x54 // 读取控制显示
#define ST7789_CMD_WRCACE       0x55 // 写入内容自适应亮度控制和色彩增强
#define ST7789_CMD_RDCABC       0x56 // 读取内容自适应亮度控制
#define ST7789_CMD_WRCABCMB     0x5E // 写入 CABC 最小亮度
#define ST7789_CMD_RDCABCMB     0x5F // 读取 CABC 最小亮度
#define ST7789_CMD_RDABCSDR     0x68 // 读取自动亮度控制诊断结果

/* 配置位掩码定义 */
// MADCTL (0x36) 掩码 [cite: 215]
#define ST7789_MADCTL_MY        0x80 // 行地址顺序 (1: Bottom to Top, 0: Top to Bottom)
#define ST7789_MADCTL_MX        0x40 // 列地址顺序 (1: Right to Left, 0: Left to Right)
#define ST7789_MADCTL_MV        0x20 // 行列交换 (1: Row/Column exchange)
#define ST7789_MADCTL_ML        0x10 // 扫描地址顺序 (0: Top to Bottom, 1: Bottom to Top)
#define ST7789_MADCTL_RGB       0x08 // RGB/BGR 顺序 (0: RGB, 1: BGR)
#define ST7789_MADCTL_MH        0x04 // 横向更新顺序 (0: Left to Right, 1: Right to Left)

// COLMOD (0x3A) 掩码 [cite: 174]
#define ST7789_COLMOD_RGB_MASK  0x70 // RGB 接口格式掩码
#define ST7789_COLMOD_CTRL_MASK 0x07 // 控制接口格式掩码
#define ST7789_COLMOD_12BIT     0x03 // 12-bit/pixel
#define ST7789_COLMOD_16BIT     0x05 // 16-bit/pixel
#define ST7789_COLMOD_18BIT     0x06 // 18-bit/pixel

// WRCTRLD (0x53) 掩码 [cite: 159]
#define ST7789_CTRLD_BCTRL      0x20 // 亮度控制开启
#define ST7789_CTRLD_DD         0x08 // 显示变暗开启
#define ST7789_CTRLD_BL         0x04 // 背光控制开启

/* 扩展寄存器地址定义 */
#define ST7789_CMD_RAMCTRL      0xB0 // RAM 控制
#define ST7789_CMD_RGBCTRL      0xB1 // RGB 接口控制
#define ST7789_CMD_PORCTRL      0xB2 // 门廊设置 (Porch Setting)
#define ST7789_CMD_FRCTRL1      0xB3 // 帧率控制 1 (局部模式)
#define ST7789_CMD_PARCTRL      0xB5 // 局部模式控制
#define ST7789_CMD_GCTRL        0xB7 // Gate 控制
#define ST7789_CMD_GTADJ        0xB8 // Gate 开启正时调整
#define ST7789_CMD_DGMEN        0xBA // 数字伽马使能
#define ST7789_CMD_VCOMS        0xBB // VCOM 设置
#define ST7789_CMD_LCMCTRL      0xC0 // LCM 控制
#define ST7789_CMD_IDSET        0xC1 // ID 代码设置
#define ST7789_CMD_VDVVRHEN     0xC2 // VDV 和 VRH 命令使能
#define ST7789_CMD_VRHS         0xC3 // VRH 设置
#define ST7789_CMD_VDVS         0xC4 // VDV 设置
#define ST7789_CMD_VCMOFSET     0xC5 // VCOM 偏移设置
#define ST7789_CMD_FRCTRL2      0xC6 // 帧率控制 (普通模式)
#define ST7789_CMD_CABCCTRL     0xC7 // CABC 控制
#define ST7789_CMD_REGSEL1      0xC8 // 寄存器值选择 1
#define ST7789_CMD_REGSEL2      0xCA // 寄存器值选择 2
#define ST7789_CMD_PWMFRSEL     0xCC // PWM 频率选择
#define ST7789_CMD_PWCTRL1      0xD0 // 电源控制 1
#define ST7789_CMD_VAPVANEN     0xD2 // VAP/VAN 信号输出使能
#define ST7789_CMD_CMD2EN       0xDF // 命令 2 使能
#define ST7789_CMD_PVGAMCTRL    0xE0 // 正电压伽马控制
#define ST7789_CMD_NVGAMCTRL    0xE1 // 负电压伽马控制
#define ST7789_CMD_DGMLUTR      0xE2 // 红色数字伽马查找表
#define ST7789_CMD_DGMLUTB      0xE3 // 蓝色数字伽马查找表
#define ST7789_CMD_GATECTRL     0xE4 // Gate 控制
#define ST7789_CMD_SPI2EN       0xE7 // SPI2 使能
#define ST7789_CMD_PWCTRL2      0xE8 // 电源控制 2
#define ST7789_CMD_EQCTRL       0xE9 // 均衡时间控制
#define ST7789_CMD_PROMCTRL     0xEC // 编程模式控制
#define ST7789_CMD_PROMEN       0xFA // 编程模式使能
#define ST7789_CMD_NVMSET       0xFC // NVM 设置
#define ST7789_CMD_PROMACT      0xFE // 编程操作

#endif