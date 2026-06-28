#ifndef __UART_PROTOCOL_H
#define __UART_PROTOCOL_H

#include "main.h"

/* ============================================================
 * 串口通信协议定义
 * 下行帧 (PC→STM32): [0xAA] [CMD] [LEN] [DATA×LEN] [XOR校验]
 * 上行帧 (STM32→PC): ACK [0xBB] [CMD] [0x01] [STATUS] [XOR]
 *                    数据帧: VOFA+ FireWater格式 (见vofa_protocol.c)
 * ============================================================ */

/* 帧头定义 */
#define FRAME_HEAD_DOWN     0xAA  /* 下行帧头 */
#define FRAME_HEAD_ACK      0xBB  /* ACK帧头 */

/* DMA接收缓冲区大小 */
#define UART_RX_BUF_SIZE    64

/* 命令字定义 */
#define CMD_SET_MODE        0x01  /* 切换工作模式: DATA[0]=0x00自动/0x01手动 */
#define CMD_SET_THRESH_H    0x02  /* 设置光照上限阈值: DATA[0~1]=uint16大端(lux) */
#define CMD_SET_THRESH_L    0x03  /* 设置光照下限阈值: DATA[0~1]=uint16大端(lux) */
#define CMD_CTRL_CURTAIN    0x04  /* 手动控制窗帘: DATA[0]=0x00开/0x01关/0x02停 */
#define CMD_CTRL_WINDOW     0x05  /* 手动控制窗户: DATA[0]=0x00开/0x01关 */
#define CMD_QUERY_STATUS    0x06  /* 查询当前状态: 无数据, 触发一次状态上报 */

/* ACK状态码 */
#define ACK_OK              0x00  /* 成功 */
#define ACK_FAIL            0x01  /* 失败 (参数错误等) */
#define ACK_RAIN_REJECT     0x02  /* 雨天拒绝开窗 */

/* DMA接收缓冲区 (在uart_protocol.c中定义, 外部可引用) */
extern uint8_t g_uart_rx_buf[UART_RX_BUF_SIZE];
extern volatile uint8_t g_uart_idle_flag;  /* IDLE中断标志 */

/* 函数声明 */
void UART_Protocol_Init(void);
void UART_Protocol_Process(void);        /* 主循环中调用, 解析收到的帧 */
void UART_SendACK(uint8_t cmd, uint8_t status);

#endif /* __UART_PROTOCOL_H */
