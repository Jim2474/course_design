/**
 * @file    uart_protocol.c
 * @brief   串口通信协议解析与ACK发送
 * @details 下行帧格式: [0xAA][CMD][LEN][DATA×LEN][XOR]
 *          XOR = CMD ^ LEN ^ DATA[0] ^ DATA[1] ^ ...
 *          接收机制: DMA Circular模式 + UART IDLE中断
 *          发送机制: HAL_UART_Transmit_IT 中断发送 + 环形发送队列
 */
#include "uart_protocol.h"
#include "usart.h"
#include "control_logic.h"

/* ============================================================
 *  中断发送队列 (HAL_UART_Transmit_IT 非阻塞发送)
 * ============================================================ */
#define UART_TX_BUF_SIZE    256     /* 发送环形缓冲区大小 */

typedef struct
{
    uint8_t  buf[UART_TX_BUF_SIZE]; /* 环形缓冲区 */
    volatile uint16_t head;         /* 写入位置 */
    volatile uint16_t tail;         /* 读取位置 */
    volatile uint8_t  busy;         /* 正在中断发送中 */
} UartTxQueue_t;

static UartTxQueue_t s_tx_queue;

/**
 * @brief  计算发送队列已用字节数
 */
static uint16_t UART_TX_Used(void)
{
    return (uint16_t)((s_tx_queue.head + UART_TX_BUF_SIZE - s_tx_queue.tail) % UART_TX_BUF_SIZE);
}

/**
 * @brief  计算发送队列空闲字节数 (保留1字节用于区分空/满)
 */
static uint16_t UART_TX_Free(void)
{
    return (uint16_t)(UART_TX_BUF_SIZE - 1 - UART_TX_Used());
}

/**
 * @brief  启动一次中断发送
 * @note   从当前tail位置发送一段连续数据, 优先处理绕回前的尾部数据
 */
static void UART_TX_Start(void)
{
    if (s_tx_queue.busy) return;
    if (s_tx_queue.head == s_tx_queue.tail) return;

    uint16_t size;
    if (s_tx_queue.head > s_tx_queue.tail)
    {
        size = s_tx_queue.head - s_tx_queue.tail;
    }
    else
    {
        size = UART_TX_BUF_SIZE - s_tx_queue.tail;
    }

    s_tx_queue.busy = 1;
    if (HAL_UART_Transmit_IT(&huart1, &s_tx_queue.buf[s_tx_queue.tail], size) != HAL_OK)
    {
        s_tx_queue.busy = 0; /* 启动失败, 下次重试 */
    }
}

/**
 * @brief  初始化发送队列
 */
void UART_TX_Init(void)
{
    s_tx_queue.head = 0;
    s_tx_queue.tail = 0;
    s_tx_queue.busy = 0;
}

/**
 * @brief  将数据加入发送队列并启动中断发送
 * @param  data  数据指针
 * @param  len   数据长度
 * @retval 1=成功入队, 0=队列空间不足
 */
uint8_t UART_TX_Send(const uint8_t *data, uint16_t len)
{
    if (len == 0 || len > UART_TX_Free()) return 0;

    for (uint16_t i = 0; i < len; i++)
    {
        s_tx_queue.buf[s_tx_queue.head] = data[i];
        s_tx_queue.head = (s_tx_queue.head + 1) % UART_TX_BUF_SIZE;
    }

    /* 若当前未在发送, 立即启动 */
    if (!s_tx_queue.busy)
    {
        UART_TX_Start();
    }
    return 1;
}

/**
 * @brief  HAL UART 发送完成回调 (中断上下文)
 * @note   更新tail位置, 继续发送队列中剩余数据
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        s_tx_queue.tail = (s_tx_queue.tail + huart->TxXferSize) % UART_TX_BUF_SIZE;
        s_tx_queue.busy = 0;
        UART_TX_Start(); /* 继续发送剩余数据 */
    }
}

/**
 * @brief  HAL UART 错误回调 (ORE/FE/NE/PE等)
 * @note   清除错误标志并重启DMA接收, 防止通信卡死
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uint32_t err = HAL_UART_GetError(huart);

        if (err & HAL_UART_ERROR_ORE)
        {
            __HAL_UART_CLEAR_OREFLAG(huart);
        }
        if (err & HAL_UART_ERROR_FE)
        {
            __HAL_UART_CLEAR_FEFLAG(huart);
        }
        if (err & HAL_UART_ERROR_NE)
        {
            __HAL_UART_CLEAR_NEFLAG(huart);
        }
        if (err & HAL_UART_ERROR_PE)
        {
            __HAL_UART_CLEAR_PEFLAG(huart);
        }

        /* 出现错误时重启DMA接收, 恢复通信 */
        HAL_UART_DMAStop(huart);
        HAL_UART_Receive_DMA(huart, g_uart_rx_buf, UART_RX_BUF_SIZE);
    }
}

/* ============================================================
 *  接收与协议解析
 * ============================================================ */

/* DMA接收缓冲区 */
uint8_t g_uart_rx_buf[UART_RX_BUF_SIZE] = {0};

/* IDLE中断标志 (在stm32f1xx_it.c的USART1_IRQHandler中置1) */
volatile uint8_t g_uart_idle_flag = 0;

/**
 * @brief  计算XOR校验值
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
 * @brief  初始化串口协议 (启动DMA接收 + IDLE中断 + 发送队列)
 * @note   必须在MX_USART1_UART_Init()之后调用
 */
void UART_Protocol_Init(void)
{
    UART_TX_Init();

    /* 启动DMA接收, 缓冲区循环使用 */
    HAL_UART_Receive_DMA(&huart1, g_uart_rx_buf, UART_RX_BUF_SIZE);
    /* 使能UART IDLE中断 */
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
}

/**
 * @brief  发送ACK帧 (使用中断发送队列)
 */
void UART_SendACK(uint8_t cmd, uint8_t status)
{
    uint8_t ack[5];
    ack[0] = FRAME_HEAD_ACK;
    ack[1] = cmd;
    ack[2] = 0x01;    /* 数据长度固定为1 */
    ack[3] = status;
    ack[4] = cmd ^ 0x01 ^ status;  /* XOR校验 */
    UART_TX_Send(ack, 5);
}

/**
 * @brief  解析一帧下行数据
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
 * @note   DMA Circular模式下用静态变量记录上次写入位置,
 *         处理缓冲区绕回情况
 */
void UART_Protocol_Process(void)
{
    if (g_uart_idle_flag == 0) return;  /* 没有新数据, 直接返回 */
    g_uart_idle_flag = 0;

    /* 静态变量: 记录上次DMA写入的缓冲区位置 */
    static uint16_t s_last_pos = 0;

    /* 计算DMA当前写入位置 */
    uint16_t dma_remain = __HAL_DMA_GET_COUNTER(huart1.hdmarx);
    uint16_t cur_pos    = UART_RX_BUF_SIZE - dma_remain;

    if (cur_pos == s_last_pos) goto done;  /* 无新数据 */

    if (cur_pos > s_last_pos)
    {
        /* 正常情况: 新数据在 [s_last_pos, cur_pos) 线性区间 */
        for (uint16_t i = s_last_pos; i < cur_pos; i++)
        {
            if (g_uart_rx_buf[i] == FRAME_HEAD_DOWN)
            {
                ParseFrame(&g_uart_rx_buf[i], cur_pos - i);
                break;
            }
        }
    }
    else
    {
        /* 缓冲区绕回: 帧可能被DMA Circular切分成两段
         * 将 [s_last_pos, BUF_SIZE) 和 [0, cur_pos) 拼接成临时线性缓冲区,
         * 再解析, 避免跨边界帧丢失 */
        uint8_t  tmp_buf[UART_RX_BUF_SIZE];
        uint16_t tmp_len = 0;

        for (uint16_t i = s_last_pos; i < UART_RX_BUF_SIZE; i++)
        {
            tmp_buf[tmp_len++] = g_uart_rx_buf[i];
        }
        for (uint16_t i = 0; i < cur_pos; i++)
        {
            tmp_buf[tmp_len++] = g_uart_rx_buf[i];
        }

        for (uint16_t i = 0; i < tmp_len; i++)
        {
            if (tmp_buf[i] == FRAME_HEAD_DOWN)
            {
                ParseFrame(&tmp_buf[i], tmp_len - i);
                break;
            }
        }
    }

done:
    s_last_pos = cur_pos;  /* 更新写入位置记录 */
}
