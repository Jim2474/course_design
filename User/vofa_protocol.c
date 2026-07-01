/**
 * @file    vofa_protocol.c
 * @brief   VOFA+ 上位机数据上传 (FireWater文本协议)
 * @details FireWater文本格式: "ch0,ch1,ch2,ch3,ch4\n"
 *          VOFA+中选择FireWater协议, 添加5个通道即可自动解析
 *          通道1: 光照强度(lux)
 *          通道2: 雨滴状态 (0=无雨, 1=有雨)
 *          通道3: 窗帘状态 (0=开, 1=关, 2=运动中)
 *          通道4: 窗户状态 (0=开, 1=关)
 *          通道5: 工作模式 (0=自动, 1=手动)
 */
#include "vofa_protocol.h"
#include "usart.h"
#include "uart_protocol.h"  /* UART_TX_Send */
#include <stdio.h>   /* snprintf */

/**
 * @brief  上传一帧数据到VOFA+ (使用HAL中断发送, 非阻塞)
 * @param  lux      光照强度 (单位lux)
 * @param  rain     雨滴状态 (0.0=无雨, 1.0=有雨)
 * @param  curtain  窗帘状态 (0.0=开, 1.0=关, 2.0=运动中)
 * @param  window   窗户状态 (0.0=开, 1.0=关)
 * @param  mode     工作模式 (0.0=自动, 1.0=手动)
 */
void VOFA_Upload(float lux, float rain, float curtain, float window, float mode)
{
    /* FireWater文本格式: "ch0,ch1,ch2,ch3,ch4\n" */
    char buf[64];
    int len = snprintf(buf, sizeof(buf),
                       "%.2f,%.2f,%.2f,%.2f,%.2f\n",
                       lux, rain, curtain, window, mode);
    if (len > 0)
    {
        /* 加入中断发送队列, 非阻塞 */
        UART_TX_Send((const uint8_t *)buf, (uint16_t)len);
    }
}
