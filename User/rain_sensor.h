#ifndef __RAIN_SENSOR_H
#define __RAIN_SENSOR_H

#include "main.h"

/* 雨滴状态定义 */
#define RAIN_NONE    0  /* 无雨 */
#define RAIN_YES     1  /* 有雨 */

/* 函数声明 */
void Rain_Init(void);
uint8_t Rain_Read(void);

#endif /* __RAIN_SENSOR_H */
