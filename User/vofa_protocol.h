#ifndef __VOFA_PROTOCOL_H
#define __VOFA_PROTOCOL_H

#include "main.h"

/* VOFA+ FireWater协议通道定义 */
/* CH1: 光照强度(lux) CH2: 雨滴状态 CH3: 窗帘状态 CH4: 窗户状态 CH5: 工作模式 */
#define VOFA_CHANNEL_NUM    5

/* 函数声明 */
void VOFA_Upload(float lux, float rain, float curtain, float window, float mode);

#endif /* __VOFA_PROTOCOL_H */
