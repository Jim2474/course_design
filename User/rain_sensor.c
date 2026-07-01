/**
 * @file    rain_sensor.c
 * @brief   雨滴传感器驱动
 * @details 使用PA0 GPIO数字输入 (DO引脚)
 *          传感器逻辑: DO=低电平 表示检测到雨水 (大多数模块为active-low)
 *          GPIO配置为PULLDOWN, 无传感器时默认为低(有雨保护)
 */
#include "rain_sensor.h"
/* LOGICSTATE已接PA0: 无需代码仿真, 直接读GPIO */

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
    /* LOGICSTATE → PA0:
     *   LOGICSTATE=0 (低电平) → 有雨 → 强制关窗
     *   LOGICSTATE=1 (高电平) → 无雨 → 自动开窗 */
    if (HAL_GPIO_ReadPin(Rain_sensor_GPIO_Port, Rain_sensor_Pin) == GPIO_PIN_RESET)
    {
        return RAIN_YES;   /* 低电平: 有雨 */
    }
    else
    {
        return RAIN_NONE;  /* 高电平: 无雨 */
    }
}
