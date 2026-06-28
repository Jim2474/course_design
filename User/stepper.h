#ifndef __STEPPER_H
#define __STEPPER_H

#include "main.h"

/* 步进电机步数定义 (28BYJ-48, 减速比1:64, 8拍/圈=512步) */
/* 实际步数根据窗帘传动比实验标定, 此处为参考值 */
#define STEPPER_STEPS_FULL   4096   /* 窗帘全行程步数 (8拍*512圈=4096步) */
#define STEPPER_SPEED_MS     2      /* 每步时间间隔(ms), 越小越快, 最小1ms */

/* 步进电机状态 */
#define STEPPER_STOP    0
#define STEPPER_OPEN    1  /* 正转 - 开帘方向 */
#define STEPPER_CLOSE   2  /* 反转 - 关帘方向 */

/* 窗帘位置状态 */
#define CURTAIN_STATE_OPEN    0
#define CURTAIN_STATE_CLOSED  1
#define CURTAIN_STATE_MOVING  2

/* 函数声明 */
void Stepper_Init(void);
void Stepper_Open(void);            /* 开始开帘 */
void Stepper_Close(void);           /* 开始关帘 */
void Stepper_Stop(void);            /* 立即停止 */
void Stepper_TIM_Callback(void);    /* 在TIM3中断中调用 */
uint8_t Stepper_GetCurtainState(void);

#endif /* __STEPPER_H */
