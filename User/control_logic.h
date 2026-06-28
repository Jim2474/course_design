#ifndef __CONTROL_LOGIC_H
#define __CONTROL_LOGIC_H

#include "main.h"

/* 工作模式定义 */
#define MODE_AUTO    0  /* 自动模式: 传感器驱动控制 */
#define MODE_MANUAL  1  /* 手动模式: 等待串口指令或按键 */

/* 光照阈值默认值 (lux) */
#define THRESHOLD_HIGH_DEFAULT  800.0f  /* 超过此值→关帘 */
#define THRESHOLD_LOW_DEFAULT   300.0f  /* 低于此值→开帘 */

/* 传感器采集周期 (TIM4中断计数, 每次中断500ms) */
#define SENSOR_PERIOD_COUNT     1   /* 1×500ms=500ms采集一次 */

/* 函数声明 */
void Control_Init(void);                         /* 初始化控制逻辑 */
void Control_TIM4_Callback(void);                /* TIM4中断回调 (500ms) */
void Control_KeyProcess(uint8_t key_event);      /* 按键事件处理 */

/* 串口协议调用的接口 */
void Control_SetMode(uint8_t mode);
void Control_SetThresholdHigh(uint16_t thresh);
void Control_SetThresholdLow(uint16_t thresh);
void Control_ManualCurtain(uint8_t action);      /* 0=开,1=关,2=停 */
uint8_t Control_ManualWindow(uint8_t action);   /* 0=开,1=关; 返回ACK状态 */
void Control_ForceUpload(void);                  /* 立即触发一次VOFA+上传 */

#endif /* __CONTROL_LOGIC_H */
