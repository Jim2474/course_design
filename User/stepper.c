/**
 * @file    stepper.c
 * @brief   28BYJ-48步进电机驱动 (4相8拍)
 * @details 使用PA4~PA7连接ULN2003驱动板
 *          TIM3每1ms中断推进一步 (speed=1)
 *          通过控制运行步数实现窗帘全开/全关
 */
#include "stepper.h"
#include "tim.h"

/* 4相8拍步序 (IN1=PA4, IN2=PA5, IN3=PA6, IN4=PA7) */
static const uint8_t STEP_SEQ[8] = {
    0x01,  /* 0001: IN1 */
    0x03,  /* 0011: IN1+IN2 */
    0x02,  /* 0010: IN2 */
    0x06,  /* 0110: IN2+IN3 */
    0x04,  /* 0100: IN3 */
    0x0C,  /* 1100: IN3+IN4 */
    0x08,  /* 1000: IN4 */
    0x09,  /* 1001: IN4+IN1 */
};

/* 内部状态变量 */
static volatile int32_t  s_target_steps  = 0;    /* 剩余目标步数 */
static volatile int8_t   s_direction     = 0;    /* 方向: +1正转, -1反转, 0停止 */
static volatile uint8_t  s_seq_index     = 0;    /* 当前步序索引 */
static volatile uint8_t  s_step_counter  = 0;    /* 速度控制计数 */
static volatile uint8_t  s_curtain_state = CURTAIN_STATE_OPEN;  /* 窗帘位置状态 */

/**
 * @brief  将步序输出到GPIO (IN1~IN4对应PA4~PA7)
 */
static void Stepper_OutputStep(uint8_t seq)
{
    /* 同时设置PA4~PA7, 先全清再置位 */
    GPIOA->ODR = (GPIOA->ODR & ~(0x0F << 4)) | ((seq & 0x0F) << 4);
}

/**
 * @brief  初始化步进电机 (关断所有线圈)
 */
void Stepper_Init(void)
{
    s_target_steps  = 0;
    s_direction     = 0;
    s_seq_index     = 0;
    s_step_counter  = 0;
    s_curtain_state = CURTAIN_STATE_OPEN;  /* 上电默认认为窗帘是开的 */
    Stepper_OutputStep(0);  /* 关断所有线圈节省电流 */
}

/**
 * @brief  开始开帘 (正转 STEPPER_STEPS_FULL 步)
 */
void Stepper_Open(void)
{
    if (s_curtain_state == CURTAIN_STATE_OPEN) return;  /* 已经是开的, 不动作 */
    s_direction    = +1;
    s_target_steps = STEPPER_STEPS_FULL;
    s_curtain_state = CURTAIN_STATE_MOVING;
}

/**
 * @brief  开始关帘 (反转 STEPPER_STEPS_FULL 步)
 */
void Stepper_Close(void)
{
    if (s_curtain_state == CURTAIN_STATE_CLOSED) return;  /* 已经是关的, 不动作 */
    s_direction    = -1;
    s_target_steps = STEPPER_STEPS_FULL;
    s_curtain_state = CURTAIN_STATE_MOVING;
}

/**
 * @brief  立即停止电机
 */
void Stepper_Stop(void)
{
    s_target_steps = 0;
    s_direction    = 0;
    Stepper_OutputStep(0);  /* 关断线圈 */
}

/**
 * @brief  获取窗帘当前状态
 * @retval CURTAIN_STATE_OPEN / CURTAIN_STATE_CLOSED / CURTAIN_STATE_MOVING
 */
uint8_t Stepper_GetCurtainState(void)
{
    return s_curtain_state;
}

/**
 * @brief  TIM3中断回调 (每1ms调用一次)
 * @note   在HAL_TIM_PeriodElapsedCallback中判断htim==&htim3时调用此函数
 */
void Stepper_TIM_Callback(void)
{
    if (s_target_steps <= 0 || s_direction == 0)
    {
        /* 运动完成: 更新状态, 关断线圈 */
        if (s_target_steps <= 0 && s_direction != 0)
        {
            if (s_direction > 0)
                s_curtain_state = CURTAIN_STATE_OPEN;
            else
                s_curtain_state = CURTAIN_STATE_CLOSED;
            s_direction = 0;
            Stepper_OutputStep(0);  /* 关断线圈 */
        }
        return;
    }
    
    /* 速度控制: 每 STEPPER_SPEED_MS 次中断推进一步 */
    s_step_counter++;
    if (s_step_counter < STEPPER_SPEED_MS) return;
    s_step_counter = 0;
    
    /* 更新步序索引 */
    if (s_direction > 0)
    {
        s_seq_index = (s_seq_index + 1) % 8;  /* 正转 */
    }
    else
    {
        s_seq_index = (s_seq_index + 7) % 8;  /* 反转 */
    }
    
    /* 输出步序 */
    Stepper_OutputStep(STEP_SEQ[s_seq_index]);
    s_target_steps--;
}
