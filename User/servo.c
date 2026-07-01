/**
 * @file    servo.c
 * @brief   SG90舵机驱动 (窗户控制)
 * @details TIM2_CH2 (PA1) 输出50Hz PWM
 *          开窗: 90度 (脉冲1500us)
 *          关窗: 0度  (脉冲500us)
 */
#include "servo.h"
#include "tim.h"
#include "main.h"  /* Error_Handler */

/* 当前窗户状态 */
static uint8_t s_window_state = WINDOW_STATE_CLOSED;  /* 上电默认关窗 */

/**
 * @brief  设置舵机脉冲宽度
 * @param  pulse_us 脉冲宽度 (单位us, 有效范围500~2500)
 */
static void Servo_SetPulse(uint16_t pulse_us)
{
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pulse_us);
}

/**
 * @brief  初始化舵机 (上电默认关窗位置)
 * @note   Bug5修复: 原代码在PWM Start之前调用Servo_SetPulse, 
 *         CCR值虽然写入寄存器, 但PWM输出通道尚未激活,
 *         Proteus仿真中舵机完全无响应。
 *         修复: 在Init内部先Start PWM, 再设置脉冲, 模块自包含。
 */
void Servo_Init(void)
{
    /* 先启动PWM输出通道, 再设置初始脉冲 */
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
    Servo_SetPulse(SERVO_WINDOW_CLOSE);
    s_window_state = WINDOW_STATE_CLOSED;
    HAL_Delay(500);  /* 等待舵机到位 */
}

/**
 * @brief  打开窗户 (舵机转至90度)
 */
void Window_Open(void)
{
    if (s_window_state == WINDOW_STATE_OPEN) return;  /* 已打开, 不重复动作 */
    Servo_SetPulse(SERVO_WINDOW_OPEN);
    s_window_state = WINDOW_STATE_OPEN;
}

/**
 * @brief  关闭窗户 (舵机转至0度)
 */
void Window_Close(void)
{
    if (s_window_state == WINDOW_STATE_CLOSED) return;  /* 已关闭, 不重复动作 */
    Servo_SetPulse(SERVO_WINDOW_CLOSE);
    s_window_state = WINDOW_STATE_CLOSED;
}

/**
 * @brief  获取窗户当前状态
 * @retval WINDOW_STATE_OPEN(0) 或 WINDOW_STATE_CLOSED(1)
 */
uint8_t Servo_GetWindowState(void)
{
    return s_window_state;
}
