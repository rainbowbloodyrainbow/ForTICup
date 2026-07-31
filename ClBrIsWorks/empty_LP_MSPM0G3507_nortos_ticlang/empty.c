/**
  ******************************************************************************
  * @file    empty.c
  * @brief   单轴步进电机闭环例程入口和5ms控制时基中断
  ******************************************************************************
  * 初始化顺序：SysConfig外设 -> 电机脉冲模块 -> 编码器模块 -> 实验调度模块。
  * 主循环只负责非阻塞任务；TIMG0每5ms产生一次中断，推进编码器累计和闭环控制。
  ******************************************************************************
  */
#include "board.h"
#include "app_demo.h"
#include "encoder.h"
#include "motor.h"

/* 主程序：初始化完成后持续执行所选实验，所有动作均采用非阻塞状态机。 */
int main(void)
{
    SYSCFG_DL_init();
    Motor_Init();
    Encoder_Init();
    Demo_Init();

    NVIC_ClearPendingIRQ(TIMER_0_INST_INT_IRQN);
    NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);

    while (1) Demo_Process();
}

/**
  * TIMG0的5ms周期中断。
  * 中断内只更新软件时基、编码器累计值并执行一次闭环计算，不做串口打印。
  */
void TIMER_0_INST_IRQHandler(void)
{
    if (DL_TimerG_getPendingInterrupt(TIMER_0_INST) == DL_TIMER_IIDX_ZERO) {
        Demo_Tick5ms();
    }
}
