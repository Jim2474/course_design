#ifndef __SERVO_H
#define __SERVO_H

#include "main.h"

/* SG90舵机PWM脉冲宽度 (单位: us, TIM2 1us/计数) */
/* TIM2: PSC=71, ARR=19999, 50Hz, 1计数=1us */
#define SERVO_WINDOW_OPEN    1500   /* 1500us -> 90度 -> 窗户打开 */
#define SERVO_WINDOW_CLOSE   500    /* 500us  -> 0度  -> 窗户关闭 */

/* 窗户状态定义 */
#define WINDOW_STATE_OPEN    0
#define WINDOW_STATE_CLOSED  1

/* 函数声明 */
void Servo_Init(void);
void Window_Open(void);
void Window_Close(void);
uint8_t Servo_GetWindowState(void);

#endif /* __SERVO_H */
