# STM32G4 单片机配置文档

## 基本信息

| 项目 | 配置 |
|------|------|
| MCU 型号 | STM32G431xx (推测: STM32G431RB/CB) |
| 主频 | 170 MHz |
| 晶振 | HSE 外部晶振 |
| 电压调节 | PWR_REGULATOR_VOLTAGE_SCALE1_BOOST |

---

## 时钟配置

### 时钟树

```
HSE (外部晶振)
    │
    ├── PLL
    │   ├── M = DIV6
    │   ├── N = 85
    │   ├── P = DIV2
    │   ├── Q = DIV2
    │   └── R = DIV2  → SYSCLK = 170MHz
    │
    └── SYSCLK
        ├── HCLK = DIV1  (170 MHz)
        ├── PCLK1 = DIV1 (170 MHz)
        ├── PCLK2 = DIV1 (170 MHz)
        └── Flash Latency = 4
```

### 时钟计算公式

```
假设 HSE = 24MHz (常见配置):
VCO = HSE / M * N = 24 / 6 * 85 = 340 MHz
SYSCLK = VCO / R = 340 / 2 = 170 MHz
```

---

## 外设配置总览

| 外设 | 用途 | 状态 |
|------|------|------|
| SPI2 | LCD 显示 (ST7789) | 启用，DMA 发送 |
| TIM1 | 编码器输入 (EC11) | 启用，编码器模式 |
| TIM8 | 电源 PWM 控制 | 启用，中心对齐模式 |
| TIM7 | 系统滴答定时器 | 启用，1kHz 中断 |
| TIM16 | 辅助 PWM | 启用 |
| TIM17 | PWM + 中断 | 启用，1kHz |
| ADC1 | 模拟输入采集 | 启用，TIM8 触发 |
| ADC2 | 模拟输入采集 | 启用，TIM8 触发 |
| DAC1 | 模拟输出 | 启用 |
| DAC3 | 模拟输出 | 启用 |
| COMP1 | 比较器保护 | 启用 |
| COMP2 | 比较器保护 | 启用 |
| COMP4 | 比较器保护 | 启用 |
| FDCAN1 | CAN 通信 | 启用 |
| I2C3 | 外部设备通信 | 启用 |
| USART2 | 串口通信 | 启用 |
| DMA1 | SPI2 发送 | 启用，Channel1 |

---

## 详细外设配置

### 1. GPIO 配置

#### 输入引脚 (按键)

| 引脚 | 端口 | 模式 | 上拉/下拉 | 功能 |
|------|------|------|-----------|------|
| PC2 | GPIOC | Input | Pull-Up | Key_Encoder |
| PC3 | GPIOC | Input | Pull-Up | Key_Left |
| PA0 | GPIOA | Input | Pull-Up | Key_Middle |
| PA1 | GPIOA | Input | Pull-Up | Key_Right |

#### 输出引脚 (LCD & LED)

| 引脚 | 端口 | 模式 | 速度 | 功能 |
|------|------|------|------|------|
| PB11 | GPIOB | Output PP | High | LCD_DC |
| PB12 | GPIOB | Output PP | High | LCD_RST |
| PB14 | GPIOB | Output PP | High | LCD_CS |
| PA9 | GPIOA | Output PP | Low | LED/Status |
| PA10 | GPIOA | Output PP | Low | LED/Status |

#### 复用功能引脚

| 引脚 | 端口 | 复用功能 | 功能 |
|------|------|----------|------|
| PB13 | GPIOB | AF5_SPI2 | SPI2_SCK |
| PB15 | GPIOB | AF5_SPI2 | SPI2_MOSI |
| PC0 | GPIOC | AF2_TIM1 | TIM1_CH1 (Encoder) |
| PC1 | GPIOC | AF2_TIM1 | TIM1_CH2 (Encoder) |
| PA15 | GPIOA | AF2_TIM8 | TIM8_CH1 |
| PC10 | GPIOC | AF4_TIM8 | TIM8_CH1N |
| PB9 | GPIOB | AF10_TIM8 | TIM8_CH3 |
| PB5 | GPIOB | AF3_TIM8 | TIM8_CH3N |

---

### 2. SPI2 配置 (LCD)

| 参数 | 配置 |
|------|------|
| 模式 | Master |
| 方向 | 2 Lines |
| 数据位 | 8 Bit |
| 时钟极性 | Low |
| 时钟相位 | 1 Edge |
| NSS | Software |
| 波特率预分频 | DIV4 (42.5 MHz) |
| 首位 | MSB |

#### SPI2 DMA 配置

| 参数 | 配置 |
|------|------|
| DMA 通道 | DMA1_Channel1 |
| 请求 | DMA_REQUEST_SPI2_TX |
| 方向 | Memory to Peripheral |
| 外设增量 | Disable |
| 存储器增量 | Enable |
| 数据对齐 | Byte |
| 模式 | Normal |
| 优先级 | Low |

---

### 3. TIM1 配置 (编码器输入)

| 参数 | 配置 |
|------|------|
| 模式 | Encoder Mode TI12 |
| 预分频 | 0 |
| 计数模式 | Up |
| 周期 | 65535 |
| IC1 极性 | Rising |
| IC1 滤波 | 4 |
| IC2 极性 | Rising |
| IC2 滤波 | 4 |

**说明**：使用 TI12 模式，编码器 A/B 相同时输入，支持 4 倍频计数。

**初始化值**：`__HAL_TIM_SET_COUNTER(&htim1, 32768)` - 设置中点值，方便双向计数。

---

### 4. TIM8 配置 (电源 PWM)

| 参数 | 配置 |
|------|------|
| 计数模式 | Center Aligned 2 |
| 预分频 | 0 |
| 周期 | 65535 |
| 自动重载预加载 | Enable |
| 死区时间 | 15 |

#### PWM 通道

| 通道 | 引脚 | 初始占空比 | 功能 |
|------|------|------------|------|
| CH1 | PA15 | 32255 | PWM 输出 |
| CH1N | PC10 | - | 互补输出 |
| CH3 | PB9 | 32255 | PWM 输出 |
| CH3N | PB5 | - | 互补输出 |
| CH4 | - | - | ADC 触发 |

#### 刹车保护配置

| 刹车源 | 使能 | 极性 |
|--------|------|------|
| COMP1 | Enable | High |
| COMP2 | Enable | High |
| COMP1 (BRK2) | Enable | High |
| COMP2 (BRK2) | Enable | High |

---

### 5. TIM7 配置 (系统滴答)

| 参数 | 配置 |
|------|------|
| 预分频 | 169 |
| 周期 | 99 |
| 中断频率 | 170MHz / (169+1) / (99+1) = 10 kHz |

---

### 6. TIM16 / TIM17 配置

| 参数 | TIM16 | TIM17 |
|------|-------|-------|
| 预分频 | 0 | 169 |
| 周期 | 65535 | 999 |
| 模式 | PWM | PWM + 中断 |
| 频率 | - | 1 kHz |

---

### 7. ADC 配置

#### ADC1

| 参数 | 配置 |
|------|------|
| 时钟预分频 | PCLK/4 |
| 分辨率 | 12 Bit |
| 数据对齐 | Right |
| 扫描模式 | Disable |
| 连续模式 | Disable |
| 外部触发 | TIM8_TRGO |
| 通道 | IN15 (PB0), IN12 (PB1) |

#### ADC2

| 参数 | 配置 |
|------|------|
| 时钟源 | SYSCLK |
| 外部触发 | TIM8_TRGO |
| 通道 | IN13 (PA5), IN3 (PA6), IN4 (PA7), IN12 (PB2) |

---

### 8. DMA 配置

| 通道 | 请求 | 功能 |
|------|------|------|
| DMA1_Channel1 | SPI2_TX | LCD 数据传输 |

---

### 9. 其他外设

| 外设 | 状态 | 说明 |
|------|------|------|
| DAC1 | 启用 | 模拟输出 |
| DAC3 | 启用 | 模拟输出 |
| COMP1 | 启用 | 过流/过压保护 |
| COMP2 | 启用 | 过流/过压保护 |
| COMP4 | 启用 | 过流/过压保护 |
| FDCAN1 | 启用 | CAN 通信 |
| I2C3 | 启用 | 外部设备 |
| USART2 | 启用 | 调试/通信 |

---

## 启动流程

```c
int main(void)
{
    HAL_Init();
    HAL_Delay(1000);                    // 延时等待稳定
    SystemClock_Config();               // 配置 170MHz 时钟
    
    // 初始化所有外设
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_FDCAN1_Init();
    MX_TIM8_Init();
    MX_DAC1_Init();
    MX_COMP1_Init();
    MX_COMP2_Init();
    MX_DAC3_Init();
    MX_SPI2_Init();
    MX_I2C3_Init();
    MX_TIM16_Init();
    MX_TIM17_Init();
    MX_USART2_UART_Init();
    MX_COMP4_Init();
    MX_TIM1_Init();
    MX_TIM7_Init();
    MX_ADC1_Init();
    MX_ADC2_Init();
    
    // 应用层初始化
    LCD_Init();
    Button_Port_Init();
    
    // 启动外设
    HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&htim8, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Start(&htim8, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start_IT(&htim17, TIM_CHANNEL_1);
    HAL_TIM_Base_Start_IT(&htim7);
    HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);
    HAL_TIM_Base_Start(&htim1);
    
    // 设置编码器初始值
    __HAL_TIM_SET_COUNTER(&htim1, 32768);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 32255);
    __HAL_TIM_MOE_ENABLE(&htim8);
}
```

---

## 中断优先级配置

| 中断源 | 优先级 | 说明 |
|--------|--------|------|
| TIM1_TRG_COM_TIM17_IRQn | 0 | 编码器/定时器 |
| TIM7_IRQn | 0 | 系统滴答 |
| DMA1_Channel1_IRQn | 0 | SPI DMA |

---

## 关键设计要点

1. **PWM 中心对齐模式**：TIM8 使用 Center Aligned 2 模式，适合电源应用的双边沿调制
2. **硬件保护**：TIM8 刹车输入连接 COMP1/COMP2，实现硬件级过流保护
3. **ADC 同步**：ADC1/ADC2 由 TIM8_TRGO 触发，实现 PWM 同步采样
4. **编码器双向计数**：TIM1 初始值设为 32768，支持正负方向计数
5. **DMA 刷屏**：SPI2 使用 DMA 传输，提高 LCD 刷新效率
