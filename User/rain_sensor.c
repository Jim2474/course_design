/**
 * @file    rain_sensor.c
 * @brief   雨滴传感器驱动
 * @details 使用PA0 GPIO数字输入 (DO引脚)
 *          传感器逻辑: DO=低电平 表示检测到雨水 (大多数模块为active-low)
 *          GPIO配置为PULLDOWN, 无传感器时默认为低(有雨保护)
 */
#include "rain_sensor.h"

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
 *         注意: 若你的模块逻辑相反(有雨=高), 请将返回值取反
 * @retval RAIN_NONE(0) 或 RAIN_YES(1)
 */
uint8_t Rain_Read(void)
{
    /* 读取PA0电平: 低电平=有雨, 高电平=无雨 */
    if (HAL_GPIO_ReadPin(Rain_sensor_GPIO_Port, Rain_sensor_Pin) == GPIO_PIN_RESET)
    {
        return RAIN_YES;   /* 低电平: 有雨 */
    }
    else
    {
        return RAIN_NONE;  /* 高电平: 无雨 */
    }
}
