# 项目软件架构

## 1. 当前目录结构

```
Application/    - 应用层接口封装
Hardware/       - 硬件驱动层
Core/           - STM32CubeMX 生成的 HAL 代码
Drivers/        - CMSIS / HAL 库
```

## 2. 现有模块说明

| 文件 | 层级 | 作用 |
|------|------|------|
| `Hardware/button.c` | 硬件 | 通用按键状态机驱动 |
| `Hardware/ST7789.c` | 硬件 | ST7789 LCD 驱动 |
| `Hardware/font.c` | 硬件 | 中英文字体渲染数据 |
| `Application/button_port.c` | 移植 | 按键 GPIO 绑定与事件回调 |
| `Application/ST7789_SPI_Port.c` | 移植 | LCD 硬件接口封装、文本绘制 API |
| `Application/control.c` | 应用 | 电源控制主循环钩子 |

## 3. UI 交互开发计划（分层）

### 层级1：基础设施
- `encoder_port.c/h`：封装 TIM1 编码器读取，解耦寄存器操作。
- `ui_data.h`：UI 与控制层共享的数据通道。
- 修复 `control.c`：清理已删除的旧 UI 头文件引用。
层级1已经完成
### 层级2：UI 框架 & 页面管理（已完成）
- `ui_framework.c/h`：定义页面生命周期接口（Init / Enter / Exit / Tick / OnButton / Draw），提供页面切换管理器、编码器分发、按键事件缓冲。
- `UI_Tick_Handler()`：心跳句柄，已挂载到 `TIM17` 中断（1kHz），内部按 10ms 分频处理逻辑。
- `UI_Draw_Handler()`：屏幕刷新分发，放在 `main()` 主循环中执行。
- `button_port.c`：按键事件通过 `UI_NotifyButton()` 转发给框架。
- `ui_page_output.c/h`、`ui_page_control.c/h`：两个页面的最小占位实现，已注册到框架。
- `ui_framework.c/h`：
  - 页面生命周期接口（Init / Draw / Encoder / Button）。
  - 页面切换管理器。  
  - 全局 UI 状态（当前页面、当前编辑项等）。
- 按键事件通过框架钩子转发到当前页面处理。
- 留下心跳钩子进行UI状态的更新
层级2已经完成
### 层级3：页面1 - 输出设置（已完成）
- `ui_page_output.c/h`：
  - 显示并编辑目标输出电压、目标限流电流。
  - 短按编码器按键切换编辑项（`OUT_MODE_IDLE` → `EDIT_V` → `EDIT_I` → 循环）。
  - 编码器旋转在当前编辑项生效：电压步进 `0.05 V`，电流步进 `0.01 A`（带上下限钳位）。
  - 当前编辑项以色块高亮（黑字黄底）提示。
  - 大字参数显示（先用 24x12 字体占位，位置和大小已预留）。

### 层级4：页面2 - 控制参数
- `ui_page_control.c/h`：
  - 可扩展的参数表结构（方便增删参数）。
  - 目前预留 PI 参数（Kp, Ki）。
  - 附加显示：输入/输出电压电流、温度、电感电流。

### 层级5：集成优化
- 局部刷新、字体替换、ADC 测量值绑定。
