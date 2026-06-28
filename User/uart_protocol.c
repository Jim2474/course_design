/**
 * @file    uart_protocol.c
 * @brief   串口通信协议解析与ACK发送
 * @details 下行帧格式: [0xAA][CMD][LEN][DATA×LEN][XOR]
 *          XOR = CMD ^ LEN ^ DATA[0] ^ DATA[1] ^ ...
 *          接收机制: DMA Circular模式 + UART IDLE中断
 *          IDLE触发后计算已接收字节数, 解析完整帧
 */
#include "uart_protocol.h"
#include "usart.h"
#include "control_logic.h"

/* DMA接收缓冲区 */
uint8_t g_uart_rx_buf[UART_RX_BUF_SIZE] = {0};

/* IDLE中断标志 (在stm32f1xx_it.c的USART1_IRQHandler中置1) */
volatile uint8_t g_uart_idle_flag = 0;

/**
 * @brief  计算XOR校验值
 * @param  buf   数据指针
 * @param  len   数据长度
 * @retval XOR校验结果
 */
static uint8_t Calc_XOR(const uint8_t *buf, uint8_t len)
{
    uint8_t xor_val = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        xor_val ^= buf[i];
    }
    return xor_val;
}

/**
 * @brief  初始化串口协议 (启动DMA接收 + IDLE中断)
 * @note   必须在MX_USART1_UART_Init()之后调用
 */
void UART_Protocol_Init(void)
{
    /* 启动DMA接收, 缓冲区循环使用 */
    HAL_UART_Receive_DMA(&huart1, g_uart_rx_buf, UART_RX_BUF_SIZE);
    /* 使能UART IDLE中断 */
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
}

/**
 * @brief  发送ACK帧
 * @param  cmd    对应的命令字
 * @param  status ACK_OK / ACK_FAIL / ACK_RAIN_REJECT
 */
void UART_SendACK(uint8_t cmd, uint8_t status)
{
    uint8_t ack[5];
    ack[0] = FRAME_HEAD_ACK;
    ack[1] = cmd;
    ack[2] = 0x01;    /* 数据长度固定为1 */
    ack[3] = status;
    ack[4] = cmd ^ 0x01 ^ status;  /* XOR校验 */
    HAL_UART_Transmit(&huart1, ack, 5, 50);
}

/**
 * @brief  解析一帧下行数据
 * @param  frame  帧数据指针 (从帧头0xAA开始)
 * @param  len    帧总长度
 */
static void ParseFrame(const uint8_t *frame, uint8_t len)
{
    /* 最小帧长度: 帧头(1)+CMD(1)+LEN(1)+校验(1) = 4 */
    if (len < 4) return;
    if (frame[0] != FRAME_HEAD_DOWN) return;

    uint8_t cmd      = frame[1];
    uint8_t data_len = frame[2];

    /* 帧总长度验证: 4 + data_len */
    if (len < (4 + data_len)) return;

    /* XOR校验: CMD^LEN^DATA... */
    uint8_t xor_check = Calc_XOR(&frame[1], 2 + data_len);
    if (xor_check != frame[3 + data_len]) return;  /* 校验失败 */

    const uint8_t *data = &frame[3];  /* 数据段指针 */

    /* 根据命令字分发处理 */
    switch (cmd)
    {
        case CMD_SET_MODE:
            if (data_len >= 1)
            {
                Control_SetMode(data[0]);  /* 0x00=自动, 0x01=手动 */
                UART_SendACK(cmd, ACK_OK);
            }
            else { UART_SendACK(cmd, ACK_FAIL); }
            break;

        case CMD_SET_THRESH_H:
            if (data_len >= 2)
            {
                uint16_t thresh = ((uint16_t)data[0] << 8) | data[1];
                Control_SetThresholdHigh(thresh);
                UART_SendACK(cmd, ACK_OK);
            }
            else { UART_SendACK(cmd, ACK_FAIL); }
            break;

        case CMD_SET_THRESH_L:
            if (data_len >= 2)
            {
                uint16_t thresh = ((uint16_t)data[0] << 8) | data[1];
                Control_SetThresholdLow(thresh);
                UART_SendACK(cmd, ACK_OK);
            }
            else { UART_SendACK(cmd, ACK_FAIL); }
            break;

        case CMD_CTRL_CURTAIN:
            if (data_len >= 1)
            {
                Control_ManualCurtain(data[0]);  /* 0=开,1=关,2=停 */
                UART_SendACK(cmd, ACK_OK);
            }
            else { UART_SendACK(cmd, ACK_FAIL); }
            break;

        case CMD_CTRL_WINDOW:
            if (data_len >= 1)
            {
                uint8_t result = Control_ManualWindow(data[0]);  /* 0=开,1=关 */
                UART_SendACK(cmd, result);  /* 可能返回ACK_RAIN_REJECT */
            }
            else { UART_SendACK(cmd, ACK_FAIL); }
            break;

        case CMD_QUERY_STATUS:
            Control_ForceUpload();  /* 立即触发一次状态上报 */
            UART_SendACK(cmd, ACK_OK);
            break;

        default:
            UART_SendACK(cmd, ACK_FAIL);  /* 未知命令 */
            break;
    }
}

/**
 * @brief  主循环中调用: 检查IDLE标志并处理接收到的数据
 * @note   DMA Circular模式下, 通过DMA计数器计算已接收字节数
 */
void UART_Protocol_Process(void)
{
    if (g_uart_idle_flag == 0) return;  /* 没有新数据, 直接返回 */
    g_uart_idle_flag = 0;

    /* 计算DMA已接收的字节数 */
    /* DMA剩余计数 = UART_RX_BUF_SIZE - 已传输字节数 */
    uint16_t dma_remain = __HAL_DMA_GET_COUNTER(huart1.hdmarx);
    uint16_t rx_len     = UART_RX_BUF_SIZE - dma_remain;

    if (rx_len == 0) return;

    /* 在缓冲区中搜索有效帧 (帧头0xAA) */
    for (uint16_t i = 0; i < rx_len; i++)
    {
        if (g_uart_rx_buf[i] == FRAME_HEAD_DOWN)
        {
            ParseFrame(&g_uart_rx_buf[i], rx_len - i);
            break;  /* 每次IDLE只处理一帧 */
        }
    }
}
