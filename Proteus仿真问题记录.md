# Proteus 仿真串口问题记录

## 问题描述

在 Proteus 8 中使用 STM32F103C8T6 模型仿真本项目时，VOFA+ 无法通过虚拟串口接收到 USART1 发送的数据。

## 测试环境

- Proteus 版本：用户本地安装版本
- STM32 模型：STM32F103C8T6
- 虚拟串口工具：Virtual Serial Port Driver（COM3-COM4 虚拟串口对）
- 上位机：VOFA+ 1.3.10，FireWater 协议，COM4，115200-8-N-1
- 代码分支：`debug/proteus-uart-fix`

## 调试过程

### Step 1：确认主循环运行

在 main 循环中添加 PA4 LED 心跳，观察到 LED 正常 500ms 闪烁。

**结论**：主循环和 SysTick 正常运行。

### Step 2：确认 TIM4 中断触发

将调试 LED 移到 PC13，在 TIM4 中断中翻转 PC13。

**结论**：TIM4 中断每 500ms 正常触发。

### Step 3：追踪 uart1_write_direct 执行

在 `uart1_write_direct` 入口处置高 PC13，出口处置低。

**结论**：`uart1_write_direct` 执行完成，没有卡在 TXE/TC 等待。

### Step 4：切换到 HSE 8MHz

将 `USE_HSE_CLOCK` 从 0 改为 1，Proteus 中 OSC Frequency 从 0 改为 8MHz，TIM 分频系数调整为 72MHz。

**现象**：PA9 出现发送波形，但 TIM4 触发周期异常。

### Step 5：分析实际时钟频率

在 Clock Scale = 8 Times 下，观察到 PA9 发送波形间隔约为 8 秒。

**推断**：
- 8 Times 下 8 秒对应真实时间 1 秒
- TIM4 周期从期望的 500ms 变为 1 秒
- 说明实际 TIM4 输入时钟约为 36MHz，而非期望的 72MHz
- 进一步推断实际 PCLK2 也可能为 36MHz
- HAL 根据 RCC 寄存器配置计算 BRR = 0x271（72MHz 下的 115200）
- 但实际 PCLK2 = 36MHz，导致实际波特率约为 56250 bps
- 与 VOFA+ 设置的 115200 严重不匹配，无法通信

### Step 6：尝试强制 BRR

强制设置 `USART1->BRR = 0x113`（对应 36MHz PCLK2 下的 115200）。

**结果**：未在用户端完成最终验证，项目决定改用真实硬件验证。

## 根本原因分析

Proteus 的 STM32F103C8T6 仿真模型在时钟处理上存在与真实硬件不一致的问题：

1. **HSI 64MHz 配置**：程序可运行，但 USART 外设无法输出数据（PA9 无波形）
2. **HSE 72MHz 配置**：USART 可输出数据，但实际外设时钟约为 36MHz，导致波特率计算基础错误
3. 模型中 RCC 寄存器配置与实际外设时钟不匹配，HAL 库基于 RCC 计算出的 BRR 无法得到正确的实际波特率

## 解决方案

**放弃 Proteus 仿真验证串口功能，改用真实硬件 + CH340 验证。**

真实 STM32F103C8T6 开发板使用外部 8MHz 晶振时：
- SYSCLK = 72MHz
- PCLK2 = 72MHz
- HAL 计算 BRR = 0x271
- 实际波特率 = 115200 bps（误差 0%）
- USART1 可正常工作

## 硬件验证方案

### 接线方式

| CH340 模块 | STM32F103C8T6 | 说明 |
|-----------|---------------|------|
| TXD       | PA10 (USART1_RX) | CH340 发送 → STM32 接收 |
| RXD       | PA9 (USART1_TX)  | STM32 发送 → CH340 接收 |
| GND       | GND              | 必须共地 |

### STM32 硬件要求

- 外部 8MHz 晶振（HSE）
- 3.3V 供电
- 晶振负载电容匹配

### VOFA+ 配置

- 协议：FireWater
- 串口：CH340 对应的 COM 口号
- 波特率：115200
- 数据位：8
- 校验位：None
- 停止位：1
- 数据流控：None

### 预期结果

- 每 500ms 收到一帧 24 字节数据
- 5 个通道分别为：光照强度、雨滴状态、窗帘状态、窗户状态、工作模式
- 数据波形正常，无乱码

## 相关 Git 提交

- `171944f debug(step1): add PA4 LED heartbeat to verify main loop is running`
- `9777241 debug(step2): add PA5 TIM4 interrupt indicator`
- `c3d7abd debug(step2-fix): move debug LED from PA4/PA5 to PC13`
- `ec2334c debug(step3): use PC13 to trace uart1_write_direct execution`
- `d10b7ed debug(step4): switch to HSE 8MHz for Proteus simulation`
- `9cfb888 debug(step5): force USART1 BRR for 36MHz PCLK2 in Proteus`

以上提交位于 `debug/proteus-uart-fix` 分支，保留用于问题追溯。

## 最终代码清理

在 `master` 分支最终保留：
- `USE_HSE_CLOCK = 1`（HSE 8MHz × 9 = 72MHz）
- TIM2/TIM3/TIM4 分频系数适配 72MHz
- 移除所有 Proteus 调试 LED 代码
- 移除强制 BRR 覆盖，恢复 HAL 自动计算

---

*记录日期：2026-07-01*
