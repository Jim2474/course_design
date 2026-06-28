# 智能窗帘与智能窗户控制系统 — 项目进度记录

> **最后更新**: 2026-06-28  
> **芯片**: STM32F103C8T6 | **IDE**: Keil MDK-ARM V5.32 | **HAL库**: FW_F1 V1.8.7  
> **上位机**: VOFA+ (FireWater协议, 115200bps)

---

## 当前进度: Phase 6 ✅ (代码集成完成, 待硬件调试)

```
Phase 1: CubeMX生成工程        ✅ 完成
Phase 2: BH1750光照传感器驱动  ✅ 完成
Phase 3: 雨滴传感器驱动        ✅ 完成
Phase 4: 步进电机驱动          ✅ 完成
Phase 5: SG90舵机驱动          ✅ 完成
Phase 6: 协议层 + 控制逻辑集成 ✅ 完成
Phase 7: 硬件联调              ⬜ 待进行
Phase 8: 按键 + 手动模式联调   ⬜ 待进行
Phase 9: 稳定性测试            ⬜ 待进行
```

---

## 项目文件结构

```
smartwindows/
├── Core/
│   ├── Src/
│   │   ├── main.c              ✅ 已集成所有模块启动和主循环
│   │   ├── stm32f1xx_it.c      ✅ 已添加UART IDLE中断处理
│   │   ├── usart.c             ✅ DMA已修复为Circular模式
│   │   ├── tim.c               ✅ TIM2/3/4配置正确
│   │   ├── gpio.c              ✅ 所有GPIO配置正确
│   │   ├── i2c.c               ✅ I2C1 400kHz Fast Mode
│   │   └── dma.c               ✅ DMA1 Channel5 (USART1_RX)
│   └── Inc/
│       └── main.h              ✅ GPIO引脚宏定义完整
│
├── User/                       ✅ 所有用户模块已创建
│   ├── bh1750.c/h              ✅ 光照传感器驱动 (I2C)
│   ├── rain_sensor.c/h         ✅ 雨滴传感器驱动 (GPIO)
│   ├── stepper.c/h             ✅ 步进电机4相8拍 (TIM3中断)
│   ├── servo.c/h               ✅ SG90舵机PWM (TIM2_CH2)
│   ├── key.c/h                 ✅ 4按键消抖扫描
│   ├── uart_protocol.c/h       ✅ 串口帧解析+ACK (DMA+IDLE)
│   ├── vofa_protocol.c/h       ✅ VOFA+ FireWater 5通道上传
│   └── control_logic.c/h       ✅ 自动/手动控制逻辑
│
├── MDK-ARM/
│   └── smartwindows.uvprojx    ✅ 已添加User组到Keil工程
│
└── smartwindows.ioc            ✅ CubeMX配置文件
```

---

## 硬件接线表

| 模块 | 引脚 | 配置 |
|---|---|---|
| 雨滴传感器 DO | PA0 | GPIO_INPUT, PULLDOWN |
| SG90 舵机 PWM | PA1 | TIM2_CH2, 50Hz, 500~1500us |
| 步进电机 IN1 | PA4 | GPIO_OUTPUT_PP, HIGH速 |
| 步进电机 IN2 | PA5 | GPIO_OUTPUT_PP, HIGH速 |
| 步进电机 IN3 | PA6 | GPIO_OUTPUT_PP, HIGH速 |
| 步进电机 IN4 | PA7 | GPIO_OUTPUT_PP, HIGH速 |
| USART1 TX | PA9 | 连接USB-TTL RXD |
| USART1 RX | PA10 | 连接USB-TTL TXD |
| KEY1 (模式切换) | PB0 | GPIO_INPUT, PULLUP |
| KEY2 (窗帘开/关) | PB1 | GPIO_INPUT, PULLUP |
| KEY3 (开窗户) | PB2 | GPIO_INPUT, PULLUP |
| KEY4 (关窗户) | PB3 | GPIO_INPUT, PULLUP |
| BH1750 SCL | PB6 | I2C1_SCL, AF_OD |
| BH1750 SDA | PB7 | I2C1_SDA, AF_OD |

---

## CubeMX已修复的Bug

| Bug | 位置 | 修复方案 | 状态 |
|---|---|---|---|
| DMA模式为Normal | usart.c:95 | 改为DMA_CIRCULAR | ✅ 已修复 |
| 无UART IDLE中断 | stm32f1xx_it.c:249 | 手动添加IDLE中断处理 | ✅ 已修复 |
| 定时器未Start | main.c:99 | 添加HAL_TIM_PWM_Start/Base_Start_IT | ✅ 已修复 |

---

## 串口通信协议

### 下行帧 (VOFA+→STM32)
`[0xAA] [CMD] [LEN] [DATA×LEN] [XOR]`

| CMD | 功能 | DATA |
|---|---|---|
| 0x01 | 切换模式 | 0x00=自动, 0x01=手动 |
| 0x02 | 设置光照上限 | 2字节uint16大端(lux) |
| 0x03 | 设置光照下限 | 2字节uint16大端(lux) |
| 0x04 | 手动控制窗帘 | 0=开,1=关,2=停 |
| 0x05 | 手动控制窗户 | 0=开,1=关 |
| 0x06 | 查询状态 | 无数据 |

### 上行帧 (STM32→VOFA+)
VOFA+ FireWater格式: 5×float + 帧尾`{0x00,0x00,0x80,0x7F}`

| 通道 | 数据 | 单位/说明 |
|---|---|---|
| CH1 | 光照强度 | lux |
| CH2 | 雨滴状态 | 0=无雨, 1=有雨 |
| CH3 | 窗帘状态 | 0=开, 1=关, 2=运动中 |
| CH4 | 窗户状态 | 0=开, 1=关 |
| CH5 | 工作模式 | 0=自动, 1=手动 |

---

## 控制逻辑摘要

```
每500ms (TIM4中断):
  1. 读BH1750光照值 → 3次滑动均值滤波
  2. 读雨滴传感器状态
  3. 雨天安全保护: 无论何种模式, 有雨→强制关窗
  4. 自动模式:
     - 光照 > 800lux → 关帘 (Stepper_Close)
     - 光照 < 300lux → 开帘 (Stepper_Open)
     - 300~800lux   → 保持当前状态 (回差控制)
     - 无雨         → 开窗
  5. 上传VOFA+ 5通道数据

主循环 (非阻塞):
  - Key_Scan() 按键消抖扫描 (每次调用不阻塞)
  - UART_Protocol_Process() 检查IDLE标志解析帧
```

---

## 待完成 (Phase 7~9)

### Phase 7: 硬件联调
- [ ] Phase 7.1: 下载程序, 验证串口Hello输出
- [ ] Phase 7.2: BH1750 I2C通信验证 (VOFA+ CH1看光照曲线)
- [ ] Phase 7.3: 雨滴传感器验证 (触摸传感器看VOFA+ CH2变化)
- [ ] Phase 7.4: 步进电机转动验证 (发0xAA 0x04 0x01 0x01 0x04 关帘)
- [ ] Phase 7.5: SG90舵机角度验证 (发0xAA 0x05 0x01 0x00 0x04 开窗)

### Phase 8: 按键 + 手动模式联调
- [ ] KEY1 切换自动/手动 (VOFA+ CH5变化)
- [ ] KEY2 手动控制窗帘
- [ ] KEY3/KEY4 手动控制窗户

### Phase 9: 稳定性测试
- [ ] 连续运行30分钟
- [ ] 多次切换模式无误动作
- [ ] 通信不丢包验证

---

## 下一个AI继续工作的指引

如果你是下一个接手这个项目的AI, 请注意:

1. **当前状态**: 所有代码已写完并集成, 工程可以编译。需要实际硬件验证。
2. **Keil编译**: 在Keil中打开 `smartwindows/MDK-ARM/smartwindows.uvprojx`, 需要在 `Target Options → C/C++ → Include Paths` 中添加 `../User` 路径。
3. **已知配置要点**: 
   - 雨滴传感器逻辑: 低电平=有雨 (active-low)。如实际相反, 修改 `rain_sensor.c` 中 GPIO_PIN_RESET/SET 判断
   - 步进电机全行程步数: `STEPPER_STEPS_FULL=4096` 是理论值, 需根据实际传动比标定
   - 舵机角度: `SERVO_WINDOW_OPEN=1500us(90°)`, `SERVO_WINDOW_CLOSE=500us(0°)`, 可能需微调
4. **待实现的功能**: 无, 所有PRD功能均已实现
5. **测试方法**: 在VOFA+发送hex帧 `AA 05 01 00 04` 开窗, `AA 05 01 01 05` 关窗

---

## 参考资料

- PRD和技术方案: [PRD_技术方案.md](../PRD_技术方案.md)
- 原理图: [原理图.png](../原理图.png)
- CubeMX配置: [smartwindows.ioc](smartwindows.ioc)
