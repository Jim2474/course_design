/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "bh1750.h"
#include "rain_sensor.h"
#include "stepper.h"
#include "servo.h"
#include "key.h"
#include "uart_protocol.h"
#include "vofa_protocol.h"
#include "control_logic.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint8_t g_tim4_flag = 0;  /* TIM4 500ms周期标志位 (中断里只置位, 主循环里处理) */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* ===== 测试代码: 最开头点亮PA4, 确认程序是否跑到main ===== */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitTypeDef test_gpio = {0};
  test_gpio.Pin = GPIO_PIN_4;
  test_gpio.Mode = GPIO_MODE_OUTPUT_PP;
  test_gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &test_gpio);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);  /* PA4输出高电平, 变红色说明跑到这里了 */
  /* ===== 测试代码结束 ===== */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  //MX_I2C1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */

  /* === 第一步: 初始化所有用户模块 (TIM中断未启动, 安全) === */
  BH1750_Init();        /* 初始化光照传感器 (I2C阻塞式, 含180ms延迟) */
  Rain_Init();          /* 初始化雨滴传感器 (GPIO已由CubeMX配置) */
  Stepper_Init();       /* 初始化步进电机 (必须在TIM3启动前) */
  Servo_Init();         /* 初始化舵机 (TIM2 PWM已Init, 上电默认关窗) */
  Key_Init();           /* 初始化按键 */
  Control_Init();       /* 初始化控制逻辑 */

  /* === 第二步: 启动定时器 (模块完整初始化后) === */
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);   /* 启动舵机PWM输出 */
  HAL_TIM_Base_Start_IT(&htim3);              /* 启动步进电机1ms定时中断 */
  HAL_TIM_Base_Start_IT(&htim4);              /* 启动500ms任务调度中断 */

  /* === 第三步: 启动串口DMA接收 (最后启动) === */
  UART_Protocol_Init(); /* 启动DMA接收 + IDLE中断 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* ===== 测试代码: PA4翻转, 确认主循环在跑 ===== */
    static uint32_t s_last_toggle = 0;
    uint32_t now = HAL_GetTick();
    if (now - s_last_toggle >= 500)  /* 每500ms翻转一次 */
    {
        s_last_toggle = now;
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_4);
    }
    /* ===== 测试代码结束 ===== */

    /* === 主循环任务 (所有阻塞操作均在此执行, 不在中断里) === */

    /* 任务1: TIM4 500ms周期任务 (传感器采集 + 控制逻辑 + VOFA+上传) */
    if (g_tim4_flag)
    {
        g_tim4_flag = 0;
        Control_TIM4_Callback();  /* 包含I2C读取, 必须在主循环中执行 */
    }

    /* 任务2: 按键扫描 (不阻塞, 轮询) */
    uint8_t key_event = Key_Scan();
    if (key_event != KEY_EVENT_NONE)
    {
        Control_KeyProcess(key_event);
    }

    /* 任务3: 串口数据解析 (检查IDLE标志) */
    UART_Protocol_Process();

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
 * @brief  TIM定时器周期到达回调函数 (TIM3: 1ms步进电机, TIM4: 500ms任务调度)
 * @note   HAL_TIM_Base_Start_IT启动后, 每次TIM溢出自动调用此函数
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        /* TIM3: 1ms中断, 驱动步进电机节拍 */
        /* 内部只有GPIO操作, 中断安全 */
        Stepper_TIM_Callback();
    }
    else if (htim->Instance == TIM4)
    {
        /* TIM4: 500ms中断, 只设标志位 */
        /* 不在中断里执行I2C/UART阻塞操作! 主循环检测g_tim4_flag后执行 */
        g_tim4_flag = 1;
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
