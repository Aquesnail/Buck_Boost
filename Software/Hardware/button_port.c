#include "button_port.h"
#include "main.h" // 确保包含 HAL 库头文件
#include "gpio.h"
#include "stm32g431xx.h"

/* 1. 定义硬件信息映射表，方便统一管理 */
typedef struct {
    GPIO_TypeDef* port;
    uint16_t      pin;
} GPIO_Config_t;

static const GPIO_Config_t btn_hw_map[BTN_HW_COUNT] = {
    {GPIOC, GPIO_PIN_2}, // ID 0: PC2
    {GPIOC, GPIO_PIN_3}, // ID 1: PC3
    {GPIOA, GPIO_PIN_1}, // ID 2: PA1
    {GPIOA, GPIO_PIN_0}, // ID 3: PA0
};

/* 2. 定义静态内存池 */
static Button_t btn_pool[BTN_HW_COUNT];

/* 3. 硬件读取回调：根据 ID 查表读取 */
static uint8_t Port_Read_Pin_Level(uint8_t id) {
    if (id < BTN_HW_COUNT) {
        return (uint8_t)HAL_GPIO_ReadPin(btn_hw_map[id].port, btn_hw_map[id].pin);
    }
    return 1; // 默认返回高电平
}

/* 4. 按键事件回调：这里处理业务逻辑 */
static void App_Button_EventHandler(uint8_t id, ButtonEvent_t event) {
    switch (id) {
        case 0: // PC2 处理
            if (event == BTN_EVENT_SINGLE_CLICK) { /* 做点什么 */ }
            break;
        case 1: // PC3 处理
            break;
        case 2: // PA1 处理
            break;
        case 3: // PA0 处理
            break;
        default:
            break;
    }
}

/* 5. 初始化：配置静态数组并注入核心模块 */
void Button_Port_Init(void) {
    for (uint8_t i = 0; i < BTN_HW_COUNT; i++) {
        btn_pool[i].id = i;
        btn_pool[i].read_pin_level = Port_Read_Pin_Level;
        btn_pool[i].event_callback = App_Button_EventHandler;
        
        // 假设你的电路是：按下时引脚接地(低电平)，则 active_level 为 0
        btn_pool[i].active_level = 0; 
        
        btn_pool[i].state = 0;
        btn_pool[i].debounce_cnt = 0;
        btn_pool[i].ticks = 0;
    }

    // 将这片静态内存交给核心逻辑层
    Button_Core_Init(btn_pool, BTN_HW_COUNT);
}

/* 6. 定时触发函数（分频处理） */
void Button_Port_Tick_Handler(void) {
    static uint8_t divider = 0;
    
    // SysTick 默认 1ms 一次，我们 10ms 扫描一次按键
    if (++divider >= 10) {
        divider = 0;
        Button_Core_Tick(10); 
    }
}