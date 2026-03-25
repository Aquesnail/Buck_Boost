#include "button_port.h"
#include "main.h"

typedef struct {
    GPIO_TypeDef* port;
    uint16_t      pin;
    const char* name; // 示例：可以把按键名称存起来
} GPIO_Config_t;

static const GPIO_Config_t btn_hw_map[BTN_HW_COUNT] = {
    {GPIOC, GPIO_PIN_2, "Key_Up"},
    {GPIOC, GPIO_PIN_3, "Key_Down"},
    {GPIOA, GPIO_PIN_1, "Key_Left"},
    {GPIOA, GPIO_PIN_0, "Key_Right"},
};

static Button_t btn_pool[BTN_HW_COUNT];

static uint8_t Port_Read_Pin_Level(uint8_t id) {
    if (id < BTN_HW_COUNT) {
        return (uint8_t)HAL_GPIO_ReadPin(btn_hw_map[id].port, btn_hw_map[id].pin);
    }
    return 1;
}

/* 核心变化点：回调函数现在接收 Button_t 指针 */
static void App_Button_EventHandler(Button_t *btn, ButtonEvent_t event) {
    // 我们可以通过 btn->id 识别按键
    // 也可以通过 (char*)btn->user_data 获取我们在初始化阶段绑定的数据
    static uint8_t i = 0;
    switch (event) {
        case BTN_EVENT_SINGLE_CLICK:
            // 示例：处理 ID 为 0 的单击
            if (btn->id == 0) { 
                i++;
            }else if(btn->id == 1){
                i++;
            }else if(btn->id == 2){
                i++;
            }else if(btn->id == 3){
                i++;
            }
            break;

        case BTN_EVENT_DOUBLE_CLICK:
            break;

        case BTN_EVENT_LONG_PRESS_START:
            break;

        case BTN_EVENT_RELEASE:
            break;

        default:
            break;
    }
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