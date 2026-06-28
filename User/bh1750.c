/**
 * @file    bh1750.c
 * @brief   BH1750FVI 数字光照传感器驱动
 * @details 使用I2C1接口 (PB6=SCL, PB7=SDA), 连续高分辨率模式
 *          量程: 1 ~ 65535 lux, 精度: 1 lux
 */
#include "bh1750.h"
#include "i2c.h"

/**
 * @brief  初始化BH1750传感器
 * @note   发送上电命令和连续高分辨率模式命令
 */
void BH1750_Init(void)
{
    uint8_t cmd;
    
    /* 发送上电命令 */
    cmd = BH1750_POWER_ON;
    HAL_I2C_Master_Transmit(&hi2c1, BH1750_ADDR, &cmd, 1, 100);
    HAL_Delay(10);
    
    /* 发送重置命令 */
    cmd = BH1750_RESET;
    HAL_I2C_Master_Transmit(&hi2c1, BH1750_ADDR, &cmd, 1, 100);
    HAL_Delay(10);
    
    /* 设置连续高分辨率模式 */
    cmd = BH1750_CONT_H_RES;
    HAL_I2C_Master_Transmit(&hi2c1, BH1750_ADDR, &cmd, 1, 100);
    HAL_Delay(180);  /* 高分辨率模式测量时间最长180ms */
}

/**
 * @brief  读取BH1750光照强度
 * @param  lux_out  输出光照强度指针 (单位: lux)
 * @retval 0=成功, -1=I2C读取失败
 */
int BH1750_Read(float *lux_out)
{
    uint8_t buf[2] = {0};
    
    /* 读取2字节原始数据 */
    if (HAL_I2C_Master_Receive(&hi2c1, BH1750_ADDR, buf, 2, 100) != HAL_OK)
    {
        return -1;  /* I2C读取失败 */
    }
    
    /* 计算光照强度: lux = (高字节<<8 | 低字节) / 1.2 */
    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
    *lux_out = (float)raw / 1.2f;
    
    return 0;
}
