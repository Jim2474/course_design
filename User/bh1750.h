#ifndef __BH1750_H
#define __BH1750_H

#include "main.h"

/* BH1750 I2C地址 (ADDR引脚接GND时地址为0x23, HAL库需左移1位为0x46) */
#define BH1750_ADDR         (0x23 << 1)

/* BH1750 指令定义 */
#define BH1750_POWER_ON     0x01  /* 上电 */
#define BH1750_RESET        0x07  /* 重置 */
#define BH1750_CONT_H_RES   0x10  /* 连续高分辨率模式 (1 lux精度) */

/* 函数声明 */
void    BH1750_Init(void);
int     BH1750_Read(float *lux_out);

#endif /* __BH1750_H */
