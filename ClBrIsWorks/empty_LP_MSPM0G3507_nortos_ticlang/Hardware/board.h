/**
 * board.h - 板级支持包 / Board Support Package
 * 系统时钟、延时、类型定义等基础功能
 * SysTick, delay, type definitions and basic utilities
 */
#ifndef _BOARD_H_
#define _BOARD_H_
#include "stdio.h"
#include "string.h"
#include "ti_msp_dl_config.h"
#include "motor.h"

/* 绝对值 / Absolute value */
#define ABS(a)      (a>0 ? a:(-a))
/* 有符号类型简写 / Signed type aliases */
typedef int32_t  s32;
typedef int16_t s16;
typedef int8_t  s8;

typedef const int32_t sc32;  /*!< Read Only */
typedef const int16_t sc16;  /*!< Read Only */
typedef const int8_t sc8;   /*!< Read Only */

/* 易失性有符号 / Volatile signed */
typedef __IO int32_t  vs32;
typedef __IO int16_t  vs16;
typedef __IO int8_t   vs8;

typedef __I int32_t vsc32;  /*!< Read Only */
typedef __I int16_t vsc16;  /*!< Read Only */
typedef __I int8_t vsc8;   /*!< Read Only */

/* 无符号类型简写 / Unsigned type aliases */
typedef uint32_t  u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef const uint32_t uc32;  /*!< Read Only */
typedef const uint16_t uc16;  /*!< Read Only */
typedef const uint8_t uc8;   /*!< Read Only */

/* 易失性无符号 / Volatile unsigned */
typedef __IO uint32_t  vu32;
typedef __IO uint16_t vu16;
typedef __IO uint8_t  vu8;

typedef __I uint32_t vuc32;  /*!< Read Only */
typedef __I uint16_t vuc16;  /*!< Read Only */
typedef __I uint8_t vuc8;   /*!< Read Only */

// 旧底盘模板枚举，步进云台当前未使用 / Legacy chassis enum, unused by this gimbal project
typedef enum 
{
	Mec_Car = 0, 
	Omni_Car, 
	Akm_Car, 
	Diff_Car, 
	FourWheel_Car, 
	Tank_Car
} CarMode;

// SysTick最大计数值，24位 / 24-bit max tick
#define SysTickMAX_COUNT 0xFFFFFF

// SysTick计数频率 80MHz / Count frequency
#define SysTickFre 80000000

// 计数值→时间单位换算 / Tick count to time unit
#define SysTick_MS(x)  ((SysTickFre/1000U)*(uint32_t)(x))
#define SysTick_US(x)  ((SysTickFre/1000000U)*(uint32_t)(x))

uint32_t Systick_getTick(void);
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);
void delay_1us(unsigned long __us);
void delay_1ms(unsigned long ms);
#endif  /* #ifndef _BOARD_H_ */
