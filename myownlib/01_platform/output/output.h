#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

#include <ti/driverlib/dl_gpio.h>
#include <ti/driverlib/dl_timera.h>
#include <ti/driverlib/dl_timerg.h>

/*
 * PWM 占空比使用千分比：
 *     0    =   0%
 *     500  =  50%
 *     1000 = 100%
 */
#define PWM_OUTPUT_DUTY_MAX (1000U)

/*
 * 引脚方向和初始电平应先在 SysConfig 中配置，并由 SYSCFG_DL_init() 初始化。
 */
void DigitalOutput_High(GPIO_Regs *port, uint32_t pin);
void DigitalOutput_Low(GPIO_Regs *port, uint32_t pin);
void DigitalOutput_Write(GPIO_Regs *port, uint32_t pin, bool high);
void DigitalOutput_Toggle(GPIO_Regs *port, uint32_t pin);

/*
 * Start 和 Stop 控制的是整个定时器。
 * 如果多个 PWM 通道共用同一个定时器，它们会一起启动或停止。
 * Stop 只停止计数；如需让某个通道持续输出低电平，应设置 duty = 0。
 */
void PwmOutput_Start(GPTIMER_Regs *timer);
void PwmOutput_Stop(GPTIMER_Regs *timer);

/*
 * 设置边沿对齐向下计数、非反相 PWM 的高电平占空比。
 * timer 的周期、时钟、输出通道和引脚复用应先在 SysConfig 中配置。
 * channel 使用 SysConfig 生成的通道宏，或对应定时器的
 * DL_TIMERA_* / DL_TIMERG_* 通道宏。
 * duty 的有效范围为 0～PWM_OUTPUT_DUTY_MAX；超出时按最大值处理。
 */
void PwmOutput_SetDuty(
    GPTIMER_Regs *timer, DL_TIMER_CC_INDEX channel, uint16_t duty);

#endif
