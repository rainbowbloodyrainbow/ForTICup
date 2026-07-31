/**
 * board.c - 板级基础功能 / Board-level utilities
 * SysTick延时、串口printf重定向等
 * Delay functions, printf retarget to UART
 */
#include "ti_msp_dl_config.h"
#include "board.h"

volatile unsigned long tick_ms;
volatile uint32_t start_time;


void SysTick_Init(void)
{
    DL_SYSTICK_config(CPUCLK_FREQ/1000);
    NVIC_SetPriority(SysTick_IRQn, 0);
}

// 读取SysTick当前计数值 / Read current SysTick count
uint32_t Systick_getTick(void)
{
	return (SysTick->VAL);
}


// ms级阻塞延时 / Blocking delay in ms
void delay_ms(uint32_t ms)
{
	// 超出最大可延时范围则截断 / Clamp to max possible delay
	//if( ms > SysTickMAX_COUNT/(SysTickFre/1000) ) ms = SysTickMAX_COUNT/(SysTickFre/1000);
	for(int i=0;i<1000;i++)
	{
		delay_us(ms);
	}
}


// us级阻塞延时 / Blocking delay in us
void delay_us(uint32_t us)
{
	// 截断超范围值 / Clamp if exceeds max
	if( us > SysTickMAX_COUNT/(SysTickFre/1000000) ) us = SysTickMAX_COUNT/(SysTickFre/1000000);

	us = us*(SysTickFre/1000000); // 单位转换为计数值 / Convert to tick count

	// 记录已经走过的时间 / Accumulated elapsed ticks
	uint32_t runningtime = 0;

	// 记录起始时刻的计数值 / Capture starting tick
	uint32_t InserTick = Systick_getTick();

	// 轮询中实时刷新 / Live tick during polling
	uint32_t tick = 0;

	uint8_t countflag = 0;
	// 等待延时结束 / Wait for delay to expire
	while(1)
	{
		tick = Systick_getTick();// 刷新当前计数值 / Refresh current tick

		// 出现下溢翻转，切换计算方式 / Handle wrap-around
		if( tick > InserTick ) countflag = 1;

		if( countflag ) runningtime = InserTick + SysTickMAX_COUNT - tick;
		else runningtime = InserTick - tick;

		if( runningtime>=us ) break;
	}

}

void delay_1us(unsigned long __us){ delay_us(__us); }
void delay_1ms(unsigned long ms){ delay_ms(ms); }

#if !defined(__MICROLIB)
// 未使用微库时需要手动补全底层函数 / Needed when not using microlib
#if (__ARMCLIB_VERSION <= 6000000)
// AC5编译器需要定义FILE结构体 / AC5 compiler needs FILE struct
struct __FILE
{
	int handle;
};
#endif

FILE __stdout;

// 禁用半主机模式 / Disable semihosting
void _sys_exit(int x)
{
	x = x;
}
#endif

// printf重定向到串口0 / Redirect printf to UART0
int fputc(int ch, FILE *stream)
{
	// 忙等直到串口空闲再发送 / Wait until UART is ready
	while( DL_UART_isBusy(UART_0_INST) == true );

	DL_UART_Main_transmitData(UART_0_INST, ch);

	return ch;
}
/*
 * TI运行库的printf会用fputc输出普通字符和%s，
 * 用fputs输出已经转换完成的整数、长整数和浮点数字符串。
 * 两个接口必须重定向到同一个UART，否则会出现文字正常而数字丢失。
 */
int fputs(const char *text, FILE *stream)
{
    while (*text != '\0') {
        if (fputc((unsigned char)*text++, stream) == EOF) return EOF;
    }
    return 0;
}
