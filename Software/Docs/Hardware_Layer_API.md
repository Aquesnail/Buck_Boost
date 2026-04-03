# Hardware & Application Layer API 文档

## 目录结构

```
Application/    - 应用层接口封装
Hardware/       - 硬件驱动层
```

---

## 一、Hardware 层

### 1. button.c / button.h - 按键核心驱动

**功能**：通用按键状态机驱动，支持单击、双击、长按检测。

#### 核心类型定义

```c
// 按键事件类型
typedef enum {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_SINGLE_CLICK,      // 单击确认
    BTN_EVENT_DOUBLE_CLICK,      // 双击
    BTN_EVENT_LONG_PRESS_START,  // 长按开始
    BTN_EVENT_LONG_PRESS_HOLD,   // 长按保持触发
    BTN_EVENT_RELEASE,           // 按键释放
} ButtonEvent_t;

// 按键结构体
typedef struct Button {
    uint8_t id;
    uint8_t active_level;                    // 有效电平 (0=低有效, 1=高有效)
    uint8_t (*read_pin_level)(uint8_t id);   // 硬件读引脚函数
    void (*event_callback)(struct Button *btn, ButtonEvent_t event);  // 事件回调
    void *user_data;                         // 用户自定义数据
    
    // 内部状态机变量
    uint8_t  state;
    uint8_t  debounce_cnt;
    uint16_t ticks;
    uint8_t  click_cnt;
} Button_t;
```

#### API 函数

| 函数 | 参数 | 说明 |
|------|------|------|
| `void Button_Core_Init(Button_t *btns, uint8_t count)` | btns: 按键数组, count: 按键数量 | 初始化按键核心 |
| `void Button_Core_Tick(uint16_t tick_ms)` | tick_ms: 时间步进(ms) | 按键状态机滴答，需周期性调用 |

#### 时间参数常量

```c
#define BTN_DEBOUNCE_MS     20    // 消抖时间
#define BTN_LONG_PRESS_MS   800   // 长按判定阈值
#define BTN_DOUBLE_CLICK_MS 250   // 双击判定间隔
#define BTN_HOLD_TICK_MS    100   // 长按保持触发频率
```

---

### 2. ST7789.c / ST7789.h - LCD 驱动

**功能**：ST7789 LCD 控制器驱动，支持 DMA 传输和屏幕旋转。

#### 核心类型定义

```c
// 旋转方向
typedef enum {
    ST7789_ROTATION_0 = 0,
    ST7789_ROTATION_90,
    ST7789_ROTATION_180,
    ST7789_ROTATION_270
} ST7789_Rotation_t;

// 驱动句柄
typedef struct ST7789_Handler {
    uint16_t width;        // 屏幕物理宽度
    uint16_t height;       // 屏幕物理高度
    uint16_t x_offset;     // X轴偏移
    uint16_t y_offset;     // Y轴偏移
    ST7789_Rotation_t rotation;

    // 硬件接口函数指针
    void (*Write_CS)(uint8_t state);
    void (*Write_DC)(uint8_t state);
    void (*Write_RST)(uint8_t state);
    void (*Delay_ms)(uint32_t ms);
    void (*SPI_Send)(uint8_t *data, uint16_t len);
    uint8_t (*SPI_Send_DMA)(uint8_t *data, uint16_t len);

    volatile uint8_t is_busy;
    uint16_t *disp_buffer;    // 显示缓冲区
    uint32_t buffer_size;     // 缓冲区大小(像素数)
} ST7789_Handler_t;
```

#### API 函数

| 函数 | 说明 |
|------|------|
| `void ST7789_Init(ST7789_Handler_t *handler)` | 初始化屏幕 |
| `void ST7789_SetAddressWindow(...)` | 设置绘制窗口 |
| `void ST7789_FillRect(...)` | 填充矩形 |
| `void ST7789_DrawImage(...)` | 绘制图片 |
| `void ST7789_SetRotation(...)` | 设置旋转方向 |
| `void ST7789_DMA_TransferComplete_Callback(...)` | DMA 完成回调 |

#### 预定义颜色 (RGB565)

```c
ST7789_COLOR_BLACK       0x0000
ST7789_COLOR_WHITE       0xFFFF
ST7789_COLOR_RED         0xF800
ST7789_COLOR_GREEN       0x07E0
ST7789_COLOR_BLUE        0x001F
ST7789_COLOR_YELLOW      0xFFE0
ST7789_COLOR_CYAN        0x07FF
ST7789_COLOR_MAGENTA     0xF81F
ST7789_COLOR_ORANGE      0xFA20
ST7789_COLOR_GRAY        0x8410
```

---

### 3. ST7789_Reg.h - ST7789 寄存器定义

**功能**：ST7789 控制器寄存器地址和位掩码定义。

主要包含：
- 命令寄存器 (0x00-0xFE)
- MADCTL 控制位掩码
- COLMOD 像素格式掩码
- WRCTRLD 控制位掩码

---

### 4. font.c / font.h - 字体支持

**功能**：中英文字体渲染支持。

#### 核心类型定义

```c
// ASCII 字体结构
typedef struct ASCIIFont {
    uint8_t h;          // 字高
    uint8_t w;          // 字宽
    uint8_t *chars;     // 字模数据
} ASCIIFont;

// 中文字体结构
typedef struct Font {
    uint8_t h;                    // 字高
    uint8_t w;                    // 字宽
    const uint8_t *chars;         // 字库数据(前4字节UTF8编码)
    uint8_t len;                  // 字库长度
    const ASCIIFont *ascii;       // 配套ASCII字体
} Font;

// 图片结构
typedef struct Image {
    uint8_t w;
    uint8_t h;
    const uint8_t *data;
} Image;
```

#### 预定义字体

```c
extern const ASCIIFont afont8x6;
extern const ASCIIFont afont12x6;
extern const ASCIIFont afont16x8;
extern const ASCIIFont afont24x12;
extern const Font font16x16;
```

---

## 二、Application 层

### 1. button_port.c / button_port.h - 按键硬件抽象层

**功能**：将 Hardware/button 驱动与具体硬件 GPIO 绑定。

#### 按键硬件映射

| ID | GPIO | 名称 | 功能 |
|----|------|------|------|
| 0 | PC2 | Key_Encoder | 编码器旋钮按键 |
| 1 | PC3 | Key_Down | 下键 |
| 2 | PA1 | Key_Left | 左键 |
| 3 | PA0 | Key_Right | 右键 |

#### API 函数

| 函数 | 说明 |
|------|------|
| `void Button_Port_Init(void)` | 初始化所有按键 |
| `void Button_Port_Tick_Handler(void)` | 按键扫描处理(建议10ms周期) |

#### 使用示例

```c
// 在 SysTick 中断中调用
void Button_Port_Tick_Handler(void) {
    static uint8_t divider = 0;
    if (++divider >= 10) {      // 10ms 扫描一次
        divider = 0;
        Button_Core_Tick(10);
    }
}
```

---

### 2. ST7789_SPI_Port.c / ST7789_SPI_Port.h - LCD 硬件接口层

**功能**：封装 ST7789 驱动，提供应用层友好的 API。

#### 硬件连接

| 信号 | GPIO | 说明 |
|------|------|------|
| SPI_SCK | PB13 | SPI2 时钟 |
| SPI_MOSI | PB15 | SPI2 数据 |
| LCD_CS | PB14 | 片选 |
| LCD_DC | PB11 | 数据/命令选择 |
| LCD_RST | PB12 | 复位 |

#### API 函数

| 函数 | 参数 | 说明 |
|------|------|------|
| `void LCD_Init(void)` | - | 初始化屏幕 |
| `void LCD_Clear(uint16_t color)` | color: 颜色值 | 清屏 |
| `void LCD_Fill(uint16_t x, y, w, h, uint16_t color)` | 位置/宽高/颜色 | 填充矩形 |
| `void LCD_DrawPoint(uint16_t x, y, uint16_t color)` | 坐标/颜色 | 画点 |
| `void LCD_DrawImage(uint16_t x, y, w, h, uint16_t *data)` | 位置/宽高/数据 | 绘制图片 |
| `void LCD_DrawChar(...)` | x,y,ch,font,fg,bg | 绘制ASCII字符 |
| `void LCD_DrawString(...)` | x,y,str,font,fg,bg | 绘制ASCII字符串 |
| `void LCD_DrawText(...)` | x,y,str,font,fg,bg | 绘制中英混合文本 |

#### DMA 回调

```c
void SPI_Tx_DMA_Callback(void);  // 在 SPI DMA 中断中调用
```

---

### 3. control.c / control.h - 控制层

**功能**：电源控制主循环钩子（待扩展）。

#### API 函数

| 函数 | 说明 |
|------|------|
| `void Control_Tick_Hook(void)` | 控制主循环钩子函数 |

---

## 三、文件依赖关系

```
Application/
├── button_port.c ──→ Hardware/button.c
├── ST7789_SPI_Port.c ──→ Hardware/ST7789.c
└── control.c ──→ (待定)

Hardware/
├── button.c (独立)
├── ST7789.c ──→ ST7789_Reg.h
└── font.c (独立)
```

---

## 四、使用流程

### 初始化顺序

```c
// 1. 初始化 LCD
LCD_Init();

// 2. 初始化按键
Button_Port_Init();

// 3. 启动相关外设
HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);  // 编码器
HAL_TIM_Base_Start_IT(&htim7);                   // 定时中断
```

### 主循环处理

```c
while (1) {
    // 按键扫描 (在定时中断中更佳)
    Button_Port_Tick_Handler();
    
    // UI 刷新
    UI_DrawMainScreen_Static();
    
    HAL_Delay(100);
}
```
