/**
 * @file    rain_sensor.c
 * @brief   雨滴传感器驱动
 * @details 使用PA0 GPIO数字输入 (DO引脚)
 *          传感器逻辑: DO=低电平 表示检测到雨水 (大多数模块为active-low)
 *          GPIO配置为PULLDOWN, 无传感器时默认为低(有雨保护)
 */
#include "rain_sensor.h"

/* 仿真模式开关 (=bh1750.c中的SIMULATION_MODE保持一致) */
#define SIMULATION_MODE  1

/**
 * @brief  初始化雨滴传感器 (GPIO已由CubeMX在gpio.c中配置)
 */
void Rain_Init(void)
{
    /* GPIO已在MX_GPIO_Init()中初始化, 此处无需额外操作 */
}

/**
 * @brief  读取雨滴传感器状态
 * @note   PA0 GPIO_PULLDOWN:
 *         - 无雨: 传感器DO输出高电平 -> 读到PIN_SET -> 返回RAIN_NONE
 *         - 有雨: 传感器DO输出低电平 -> 读到PIN_RESET -> 返回RAIN_YES
 *         仿真注意: PA0带下拉, Proteus中未接LOGICSTATE时默认低电平=有雨,
 *         系统会永远强制关窗, 无法验证自动开窗逻辑。
 *         如果要在Proteus中模拟无雨状态, 请将LOGICSTATE接到PA0并置为1。
 * @retval RAIN_NONE(0) 或 RAIN_YES(1)
 */
uint8_t Rain_Read(void)
{
#if SIMULATION_MODE
    /* 仿真模式: 周期切换雨天状态, 让仿真中可以看到窗户开/关的完整周期。
     * 每40次调用切换一次 (TIM4=500ms x 40 = 20秒)
     * 如果Proteus中已接LOGICSTATE到PA0, 将此宏改为0使用真实GPIO读取 */
    static uint32_t s_call_count = 0;
    s_call_count++;
    /* 前20次调用(0~10秒): 模拟无雨 | 后20次调用(10~20秒): 模拟有雨 */
    if ((s_call_count % 40) < 20)
        return RAIN_NONE;  /* 无雨: 窗户应开 */
    else
        return RAIN_YES;   /* 有雨: 窗户应关 */
#else
    /* 实体板模式: 读取PA0电平 */
    if (HAL_GPIO_ReadPin(Rain_sensor_GPIO_Port, Rain_sensor_Pin) == GPIO_PIN_RESET)
    {
        return RAIN_YES;   /* 低电平: 有雨 */
    }
    else
    {
        return RAIN_NONE;  /* 高电平: 无雨 */
    }
#endif
}
