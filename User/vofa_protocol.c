/**
 * @file    vofa_protocol.c
 * @brief   VOFA+ 上位机数据上传 (FireWater协议)
 * @details FireWater格式: N个float(小端序) + 帧尾{0x00,0x00,0x80,0x7F}
 *          VOFA+中选择FireWater协议, 添加5个通道即可自动解析
 *          通道1: 光照强度(lux)
 *          通道2: 雨滴状态 (0=无雨, 1=有雨)
 *          通道3: 窗帘状态 (0=开, 1=关, 2=运动中)
 *          通道4: 窗户状态 (0=开, 1=关)
 *          通道5: 工作模式 (0=自动, 1=手动)
 */
#include "vofa_protocol.h"
#include "usart.h"
#include <string.h>  /* memcpy */

/* FireWater帧尾标识 (IEEE 754 +Inf = 0x7F800000, 小端序) */
static const uint8_t VOFA_TAIL[4] = {0x00, 0x00, 0x80, 0x7F};

/**
 * @brief  上传一帧数据到VOFA+
 * @param  lux      光照强度 (单位lux)
 * @param  rain     雨滴状态 (0.0=无雨, 1.0=有雨)
 * @param  curtain  窗帘状态 (0.0=开, 1.0=关, 2.0=运动中)
 * @param  window   窗户状态 (0.0=开, 1.0=关)
 * @param  mode     工作模式 (0.0=自动, 1.0=手动)
 * @note   Bug8修复: 原代码分两次HAL_UART_Transmit发送数据体和帧尾。
 *         若第一次Transmit正在进行时UART仍BUSY, 第二次发帧尾会
 *         返回HAL_BUSY被丢弃, 导致VOFA+收到无帧尾的截断帧,
 *         无法定界, 显示0.1这样的异常小数值。
 *         修复: 将data(20字节)+TAIL(4字节)拼为24字节连续buffer,
 *         单次Transmit原子发送整帧, 彻底消除帧分割风险。
 */
void VOFA_Upload(float lux, float rain, float curtain, float window, float mode)
{
    /* 完整帧: 5×float(20字节) + 帧尾(4字节) = 24字节 */
    uint8_t frame[VOFA_CHANNEL_NUM * sizeof(float) + sizeof(VOFA_TAIL)];
    float data[VOFA_CHANNEL_NUM];
    data[0] = lux;
    data[1] = rain;
    data[2] = curtain;
    data[3] = window;
    data[4] = mode;

    memcpy(frame,               data,      sizeof(data));
    memcpy(frame + sizeof(data), VOFA_TAIL, sizeof(VOFA_TAIL));

    /* 单次原子发送, 避免帧分割 */
    HAL_UART_Transmit(&huart1, frame, sizeof(frame), 100);
}
