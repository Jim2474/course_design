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

/* FireWater帧尾标识 (IEEE 754 +Inf 小端序) */
static const uint8_t VOFA_TAIL[4] = {0x00, 0x00, 0x80, 0x7F};

/**
 * @brief  直接寄存器发送UART数据 (完全绕过HAL状态机)
 * @note   根因说明:
 *         HAL_UART_Receive_DMA() 启动后, HAL 内部状态可能导致
 *         HAL_UART_Transmit() 检查 gState 时返回 HAL_BUSY 而静默丢弃数据。
 *         直接操作 USART1->SR / USART1->DR 寄存器无状态检查,
 *         在任何 HAL 状态下均可安全发送, 是混用 DMA-RX + 阻塞-TX 的标准做法。
 * @param  buf  数据指针
 * @param  len  字节数
 */
static void uart1_write_direct(const uint8_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        /* 等待发送数据寄存器为空 (TXE: Transmit Data Register Empty) */
        while (!(USART1->SR & USART_SR_TXE)) {}
        USART1->DR = buf[i];
    }
    /* 等待最后一字节移出移位寄存器 (TC: Transmission Complete) */
    while (!(USART1->SR & USART_SR_TC)) {}
}

/**
 * @brief  上传一帧数据到VOFA+
 * @param  lux      光照强度 (单位lux)
 * @param  rain     雨滴状态 (0.0=无雨, 1.0=有雨)
 * @param  curtain  窗帘状态 (0.0=开, 1.0=关, 2.0=运动中)
 * @param  window   窗户状态 (0.0=开, 1.0=关)
 * @param  mode     工作模式 (0.0=自动, 1.0=手动)
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

    memcpy(frame,                data,      sizeof(data));
    memcpy(frame + sizeof(data), VOFA_TAIL, sizeof(VOFA_TAIL));

    /* 直接寄存器发送, 不受HAL状态机限制 */
    uart1_write_direct(frame, sizeof(frame));
}
