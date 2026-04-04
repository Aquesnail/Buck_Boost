#include "button_port.h"
#include "main.h"
#include "ui_framework.h"

typedef struct {
    GPIO_TypeDef* port;
    uint16_t      pin;
    const char* name; // 示例：可以把按键名称存起来
} GPIO_Config_t;

static const GPIO_Config_t btn_hw_map[BTN_HW_COUNT] = {
    {GPIOC, GPIO_PIN_2, "Key_Encoder"}, // EC11编码器旋钮自带的按键
    {GPIOC, GPIO_PIN_3, "Key_Left"},
    {GPIOA, GPIO_PIN_0, "Key_Middle"},
    {GPIOA, GPIO_PIN_1, "Key_Right"},
};

static Button_t btn_pool[BTN_HW_COUNT];

static uint8_t Port_Read_Pin_Level(uint8_t id) {
    if (id < BTN_HW_COUNT) {
        return (uint8_t)HAL_GPIO_ReadPin(btn_hw_map[id].port, btn_hw_map[id].pin);
    }
    return 1;
}

static void App_Button_EventHandler(Button_t *btn, ButtonEvent_t event) {
    UI_NotifyButton(btn->id, event);
}

void Button_Port_Init(void) {
    for (uint8_t i = 0; i < BTN_HW_COUNT; i++) {
        btn_pool[i].id = i;
        btn_pool[i].read_pin_level = Port_Read_Pin_Level;
        btn_pool[i].event_callback = App_Button_EventHandler;
        btn_pool[i].active_level = 0; // 低电平有效
        
        // 绑定 user_data，这里以字符串名称为例，实际项目中可以是更复杂的结构体
        btn_pool[i].user_data = (void*)btn_hw_map[i].name; 

        btn_pool[i].state = 0;
        btn_pool[i].ticks = 0;
        btn_pool[i].click_cnt = 0;
    }

    Button_Core_Init(btn_pool, BTN_HW_COUNT);
}

void Button_Port_Tick_Handler(void) {
    static uint8_t divider = 0;
    // 如果你在 SysTick(1ms) 中调用，divider >= 10 意味着 10ms 扫描一次核心逻辑
    if (++divider >= 10) {
        divider = 0;
        Button_Core_Tick(10); // 传入真实的步进毫秒数
    }
}