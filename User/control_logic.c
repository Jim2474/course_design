/**
 * @file    control_logic.c
 * @brief   系统核心控制逻辑 (自动/手动模式、回差控制、优先级管理)
 * @details 自动模式:
 *            - 雨滴触发 → 强制关窗 (最高安全优先级)
 *            - 光照 > 上限阈值 → 关帘
 *            - 光照 < 下限阈值 → 开帘
 *            - 处于回差区间 → 保持当前状态
 *          手动模式:
 *            - 等待串口指令或按键
 *            - 雨天安全保护始终有效 (即使手动也不允许开窗)
 *          TIM4 500ms定时触发传感器读取和状态上报
 */
#include "control_logic.h"
#include "bh1750.h"
#include "rain_sensor.h"
#include "stepper.h"
#include "servo.h"
#include "vofa_protocol.h"
#include "uart_protocol.h"

/* ============ 全局状态变量 ============ */
static uint8_t  g_work_mode      = MODE_AUTO;              /* 当前工作模式 */
static float    g_lux_value      = 0.0f;                   /* 当前光照强度 */
static uint8_t  g_rain_state     = RAIN_NONE;              /* 当前雨滴状态 */
static float    g_thresh_high    = THRESHOLD_HIGH_DEFAULT; /* 光照上限阈值 */
static float    g_thresh_low     = THRESHOLD_LOW_DEFAULT;  /* 光照下限阈值 */

/* 滑动平均滤波 (3次) */
#define FILTER_SIZE  3
static float g_lux_buf[FILTER_SIZE] = {0};
static uint8_t g_lux_idx = 0;

/**
 * @brief  光照滑动平均滤波
 * @param  new_val 新采样值
 * @retval 滤波后的平均值
 */
static float LuxFilter(float new_val)
{
    g_lux_buf[g_lux_idx] = new_val;
    g_lux_idx = (g_lux_idx + 1) % FILTER_SIZE;
    float sum = 0;
    for (uint8_t i = 0; i < FILTER_SIZE; i++) sum += g_lux_buf[i];
    return sum / FILTER_SIZE;
}

/**
 * @brief  初始化控制逻辑
 */
void Control_Init(void)
{
    g_work_mode   = MODE_AUTO;
    g_lux_value   = 0.0f;
    g_rain_state  = RAIN_NONE;
    g_thresh_high = THRESHOLD_HIGH_DEFAULT;
    g_thresh_low  = THRESHOLD_LOW_DEFAULT;
    g_lux_idx     = 0;
    for (uint8_t i = 0; i < FILTER_SIZE; i++) g_lux_buf[i] = 0;
}

/**
 * @brief  执行自动控制逻辑 (每500ms由TIM4回调触发)
 */
static void Control_AutoTask(void)
{
    /* === 窗户控制 (最高安全优先级) === */
    if (g_rain_state == RAIN_YES)
    {
        Window_Close();  /* 检测到降雨: 无条件关窗 */
    }
    else
    {
        Window_Open();   /* 无雨: 打开窗户 */
    }

    /* === 窗帘控制 (回差控制) === */
    if (g_lux_value > g_thresh_high)
    {
        Stepper_Close();  /* 光照超过上限: 关帘 */
    }
    else if (g_lux_value < g_thresh_low)
    {
        Stepper_Open();   /* 光照低于下限: 开帘 */
    }
    /* else: 处于回差区间 (thresh_low ~ thresh_high), 保持当前状态不动 */
}

/**
 * @brief  TIM4中断回调 (每500ms触发一次)
 * @note   在HAL_TIM_PeriodElapsedCallback中判断htim==&htim4时调用
 */
void Control_TIM4_Callback(void)
{
    /* 1. 读取传感器数据 */
    float raw_lux = 0;
    if (BH1750_Read(&raw_lux) == 0)
    {
        g_lux_value = LuxFilter(raw_lux);  /* 滑动平均滤波 */
    }
    g_rain_state = Rain_Read();

    /* 2. 雨天安全保护 (无论何种模式始终有效) */
    if (g_rain_state == RAIN_YES)
    {
        Window_Close();
    }

    /* 3. 自动模式执行控制逻辑 */
    if (g_work_mode == MODE_AUTO)
    {
        Control_AutoTask();
    }

    /* 4. 上传状态到VOFA+ */
    VOFA_Upload(
        g_lux_value,
        (float)g_rain_state,
        (float)Stepper_GetCurtainState(),
        (float)Servo_GetWindowState(),
        (float)g_work_mode
    );
}

/**
 * @brief  按键事件处理
 * @param  key_event  KEY_EVENT_KEY1~KEY4
 */
void Control_KeyProcess(uint8_t key_event)
{
    switch (key_event)
    {
        case 0:  /* KEY1: 切换自动/手动模式 */
            g_work_mode = (g_work_mode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
            break;

        case 1:  /* KEY2: 窗帘开/关切换 (仅手动模式有效) */
            if (g_work_mode == MODE_MANUAL)
            {
                if (Stepper_GetCurtainState() == CURTAIN_STATE_CLOSED)
                    Stepper_Open();
                else
                    Stepper_Close();
            }
            break;

        case 2:  /* KEY3: 打开窗户 (仅手动模式, 且无雨) */
            if (g_work_mode == MODE_MANUAL)
            {
                if (g_rain_state == RAIN_NONE)
                    Window_Open();
                /* 雨天静默拒绝 (不执行) */
            }
            break;

        case 3:  /* KEY4: 关闭窗户 */
            if (g_work_mode == MODE_MANUAL)
            {
                Window_Close();
            }
            break;

        default:
            break;
    }
}

/* ============ 串口协议接口实现 ============ */

/**
 * @brief  设置工作模式
 * @param  mode MODE_AUTO(0) 或 MODE_MANUAL(1)
 */
void Control_SetMode(uint8_t mode)
{
    if (mode == MODE_AUTO || mode == MODE_MANUAL)
        g_work_mode = mode;
}

/**
 * @brief  设置光照上限阈值
 * @param  thresh 阈值 (单位: lux)
 */
void Control_SetThresholdHigh(uint16_t thresh)
{
    g_thresh_high = (float)thresh;
}

/**
 * @brief  设置光照下限阈值
 * @param  thresh 阈值 (单位: lux)
 */
void Control_SetThresholdLow(uint16_t thresh)
{
    g_thresh_low = (float)thresh;
}

/**
 * @brief  手动控制窗帘
 * @param  action 0=开, 1=关, 2=停
 */
void Control_ManualCurtain(uint8_t action)
{
    switch (action)
    {
        case 0: Stepper_Open();  break;
        case 1: Stepper_Close(); break;
        case 2: Stepper_Stop();  break;
        default: break;
    }
}

/**
 * @brief  手动控制窗户
 * @param  action 0=开, 1=关
 * @retval ACK_OK(0x00) 或 ACK_RAIN_REJECT(0x02) (雨天拒绝开窗)
 */
uint8_t Control_ManualWindow(uint8_t action)
{
    if (action == 0)  /* 开窗请求 */
    {
        if (g_rain_state == RAIN_YES)
            return ACK_RAIN_REJECT;  /* 雨天拒绝开窗 */
        Window_Open();
    }
    else if (action == 1)  /* 关窗 */
    {
        Window_Close();
    }
    return ACK_OK;
}

/**
 * @brief  立即触发一次VOFA+状态上报 (响应查询命令)
 */
void Control_ForceUpload(void)
{
    VOFA_Upload(
        g_lux_value,
        (float)g_rain_state,
        (float)Stepper_GetCurtainState(),
        (float)Servo_GetWindowState(),
        (float)g_work_mode
    );
}
