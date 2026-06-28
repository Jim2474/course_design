/**
 * @file    key.c
 * @brief   4个按键扫描驱动 (软件消抖)
 * @details 按键均为上拉输入 (GPIO_PULLUP)
 *          按下时为低电平, 松开时为高电平
 *          消抖时间: 20ms
 */
#include "key.h"

/* 按键GPIO引脚映射 */
static const uint16_t KEY_PINS[4] = {
    KEY1_Pin, KEY2_Pin, KEY3_Pin, KEY4_Pin
};
static GPIO_TypeDef * const KEY_PORTS[4] = {
    KEY1_GPIO_Port, KEY2_GPIO_Port, KEY3_GPIO_Port, KEY4_GPIO_Port
};

/* 消抖状态机 */
typedef enum {
    KEY_IDLE = 0,    /* 按键未按下 */
    KEY_PRESSING,    /* 检测到下降沿, 等待消抖 */
    KEY_PRESSED,     /* 已确认按下 */
    KEY_RELEASING,   /* 等待松开 */
} KeyState_t;

static KeyState_t s_state[4]  = {KEY_IDLE, KEY_IDLE, KEY_IDLE, KEY_IDLE};
static uint32_t   s_tick[4]   = {0, 0, 0, 0};  /* 消抖开始时间 */

/**
 * @brief  初始化按键 (GPIO已由CubeMX配置)
 */
void Key_Init(void)
{
    /* GPIO已在MX_GPIO_Init()中初始化 */
}

/**
 * @brief  按键扫描 (在主循环中调用, 不阻塞)
 * @note   每次调用最多返回一个按键事件
 * @retval KEY_EVENT_KEY1~KEY4 (有按键触发) 或 KEY_EVENT_NONE (无事件)
 */
uint8_t Key_Scan(void)
{
    uint32_t now = HAL_GetTick();
    
    for (uint8_t i = 0; i < 4; i++)
    {
        GPIO_PinState pin = HAL_GPIO_ReadPin(KEY_PORTS[i], KEY_PINS[i]);
        
        switch (s_state[i])
        {
            case KEY_IDLE:
                if (pin == GPIO_PIN_RESET)  /* 检测到按下 (下降沿) */
                {
                    s_state[i] = KEY_PRESSING;
                    s_tick[i]  = now;
                }
                break;
                
            case KEY_PRESSING:
                if (pin == GPIO_PIN_SET)    /* 抖动, 恢复到IDLE */
                {
                    s_state[i] = KEY_IDLE;
                }
                else if ((now - s_tick[i]) >= KEY_DEBOUNCE_MS)  /* 消抖时间到 */
                {
                    s_state[i] = KEY_PRESSED;
                    return i;  /* 返回按键事件 (有效按下) */
                }
                break;
                
            case KEY_PRESSED:
                /* 等待松开 */
                s_state[i] = KEY_RELEASING;
                break;
                
            case KEY_RELEASING:
                if (pin == GPIO_PIN_SET)  /* 松开 */
                {
                    s_state[i] = KEY_IDLE;
                }
                break;
                
            default:
                s_state[i] = KEY_IDLE;
                break;
        }
    }
    
    return KEY_EVENT_NONE;
}
