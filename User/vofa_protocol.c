/**
 * @file    vofa_protocol.c
 * @brief   VOFA+ 上位机数据上传 (FireWater协议)
 * @details FireWater格式: N个float(小端序) + 帧尾{0x00,0x00,0x80,0x7F}
 *          VOFA+中选择FireWater协议, 添加5个通道即可自动解析
 *          通道1: 光照强度(lux)
 *          通道2: 雨滴状态 (0=无雨, 1=有雨)
 *          通道3: 窗帘状态 (0=开, 1=关)
 *          通道4: 窗户状态 (0=开, 1=关)
 *          通道5: 工作模式 (0=自动, 1=手动)
 */
#include "vofa_protocol.h"
#include "usart.h"

/* FireWater帧尾标识 */
static const uint8_t VOFA_TAIL[4] = {0x00, 0x00, 0x80, 0x7F};

/**
 * @brief  上传一帧数据到VOFA+
 * @param  lux      光照强度 (单位lux)
 * @param  rain     雨滴状态 (0.0=无雨, 1.0=有雨)
 * @param  curtain  窗帘状态 (0.0=开, 1.0=关)
 * @param  window   窗户状态 (0.0=开, 1.0=关)
 * @param  mode     工作模式 (0.0=自动, 1.0=手动)
 */
void VOFA_Upload(float lux, float rain, float curtain, float window, float mode)
{
    float data[VOFA_CHANNEL_NUM];
    data[0] = lux;
    data[1] = rain;
    data[2] = curtain;
    data[3] = window;
    data[4] = mode;
    
    /* 发送浮点数据 (20字节) */
    HAL_UART_Transmit(&huart1, (uint8_t *)data, sizeof(data), 100);
    /* 发送帧尾 (4字节) */
    HAL_UART_Transmit(&huart1, (uint8_t *)VOFA_TAIL, sizeof(VOFA_TAIL), 100);
}
