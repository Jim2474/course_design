#ifndef __KEY_H
#define __KEY_H

#include "main.h"

/* 按键编号 */
#define KEY1    0  /* PB0: 切换自动/手动模式 */
#define KEY2    1  /* PB1: 窗帘开/关切换 */
#define KEY3    2  /* PB2: 打开窗户 */
#define KEY4    3  /* PB3: 关闭窗户 */

/* 按键事件 */
#define KEY_EVENT_NONE      0xFF  /* 无按键事件 */
#define KEY_EVENT_KEY1      0     /* KEY1被按下 */
#define KEY_EVENT_KEY2      1     /* KEY2被按下 */
#define KEY_EVENT_KEY3      2     /* KEY3被按下 */
#define KEY_EVENT_KEY4      3     /* KEY4被按下 */

/* 消抖时间 (ms) */
#define KEY_DEBOUNCE_MS     20

/* 函数声明 */
void Key_Init(void);
uint8_t Key_Scan(void);  /* 返回KEY_EVENT_xxx, 主循环中轮询调用 */

#endif /* __KEY_H */
