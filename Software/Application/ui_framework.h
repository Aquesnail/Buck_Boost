#ifndef __UI_FRAMEWORK_H__
#define __UI_FRAMEWORK_H__

#include <stdint.h>
#include "button.h"
#include <stddef.h>
/*
 * @brief 页面生命周期接口
 * @note  所有回调均可为 NULL，由框架做保护性判断
 */
typedef struct {
    const char *name;
    void (*Init)(void);         // 注册页面时调用一次
    void (*Enter)(void);        // 切入该页面前调用
    void (*Exit)(void);         // 切出该页面后调用
    void (*Tick)(int16_t encoder_delta); // 心跳：处理输入、动画、闪烁态等
    void (*OnButton)(uint8_t btn_id, ButtonEvent_t event); // 按键事件
    void (*Draw)(void);         // 主循环调用，负责屏幕刷新
} UI_Page_t;

/*
 * @brief 初始化 UI 框架内部状态
 */
void UI_Init(void);

/*
 * @brief 注册页面表，并自动调用所有页面的 Init，随后进入第 0 页
 */
void UI_RegisterPages(UI_Page_t *pages[], uint8_t count);

/*
 * @brief 切换到指定页面索引
 */
void UI_SwitchPage(uint8_t page_idx);

/*
 * @brief 获取当前页面索引
 */
uint8_t UI_GetCurrentPageIdx(void);

/*
 * @brief 按键事件通知（由 button_port.c 在事件发生时调用）
 */
void UI_NotifyButton(uint8_t btn_id, ButtonEvent_t event);

/*
 * @brief UI 心跳句柄
 * @note  建议放在 1ms 周期的定时器中断中调用；内部按 10ms 分频处理逻辑
 */
void UI_Tick_Handler(void);

/*
 * @brief UI 屏幕刷新句柄
 * @note  必须放在主循环中调用，负责调用当前页面的 Draw
 */
void UI_Draw_Handler(void);

#endif
