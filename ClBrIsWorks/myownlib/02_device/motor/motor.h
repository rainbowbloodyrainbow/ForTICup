#ifndef MOTOR_H
#define MOTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "output.h"

/*
 * TB6612 单路电机输出使用有符号千分比：
 *     -1000 = 逻辑反转全输出
 *         0 = 短路制动
 *      1000 = 逻辑正转全输出
 *
 * “逻辑正转”由整车统一定义。若某只电机的安装方向相反，应在配置中设置
 * inverted，不要在上层控制器中交换正负号。
 */
#define MOTOR_OUTPUT_MAX (1000U)

typedef enum {
    MOTOR_MODE_UNINITIALIZED = 0,
    MOTOR_MODE_FORWARD,
    MOTOR_MODE_REVERSE,
    MOTOR_MODE_COAST,
    MOTOR_MODE_BRAKE,
    MOTOR_MODE_REVERSAL_WAIT,
} Motor_Mode;

typedef struct {
    GPTIMER_Regs *pwmTimer;
    DL_TIMER_CC_INDEX pwmChannel;

    GPIO_Regs *in1Port;
    uint32_t in1Pin;
    GPIO_Regs *in2Port;
    uint32_t in2Pin;

    /*
     * maximumOutput 的有效范围为 0～MOTOR_OUTPUT_MAX。
     * 设为 0 可在参数尚未确认时锁止该电机的有效输出。
     */
    uint16_t maximumOutput;
    bool inverted;

    /*
     * 非零输出跨越正负号时，先进入短路制动，再等待该时间后换向。
     * 设为 0 时仍会先写入制动状态，但不会额外等待控制周期。
     */
    uint32_t reversalDelayMs;
} Motor_Config;

/*
 * Motor 的字段用于保存非阻塞换向状态。应用应为左右电机各创建一个实例，
 * 只通过下面的接口操作，不应直接修改结构体字段。
 */
typedef struct {
    Motor_Config config;

    int16_t requestedOutput;
    int16_t appliedOutput;
    int16_t pendingOutput;

    uint32_t zeroStartedMs;
    int8_t lastPhysicalDirection;

    Motor_Mode mode;
    bool initialized;
    bool zeroOutputActive;
    bool reversalPending;
} Motor;

/*
 * 初始化单路电机并立即置为短路制动。
 *
 * 本函数不启动 PWM 定时器，也不拉高 TB6612 STBY。左右通道共用 TIMG0 时，
 * 应先分别初始化两个 Motor、设置安全输出，再只启动一次定时器，最后使能 STBY。
 *
 * 配置无效时返回 false，且不操作硬件。
 */
bool Motor_Init(
    Motor *motor, const Motor_Config *config, uint32_t nowMs);

/*
 * 请求有符号千分比输出。超出 maximumOutput 的幅值会被限幅。
 *
 * 正负号直接切换时不会阻塞等待：本函数先将硬件置为短路制动并记录待执行输出，
 * 应用随后在主循环持续调用 Motor_Process()。等待 reversalDelayMs 后，新方向
 * 自动生效。等待期间再次调用本函数可以更新或取消待执行命令。
 *
 * output = 0 明确定义为短路制动，不是滑行。
 */
void Motor_SetOutput(
    Motor *motor, int16_t output, uint32_t nowMs);
void Motor_Process(Motor *motor, uint32_t nowMs);

void Motor_Forward(
    Motor *motor, uint16_t output, uint32_t nowMs);
void Motor_Reverse(
    Motor *motor, uint16_t output, uint32_t nowMs);

/*
 * TB6612 单通道停止真值表：
 *   Coast：IN1 = 0，IN2 = 0，PWM = 1，输出为高阻。
 *   Brake：PWM = 0，输出为短路制动；实现同时令 IN1 = IN2 = 1。
 *
 * 两个接口都会取消尚未完成的换向请求。
 */
void Motor_Coast(Motor *motor, uint32_t nowMs);
void Motor_Brake(Motor *motor, uint32_t nowMs);

bool Motor_IsInitialized(const Motor *motor);
bool Motor_IsReversalPending(const Motor *motor);
int16_t Motor_GetRequestedOutput(const Motor *motor);
int16_t Motor_GetAppliedOutput(const Motor *motor);
Motor_Mode Motor_GetMode(const Motor *motor);

/*
 * TB6612 的 STBY 同时控制 A、B 两个 H 桥，因此独立于单路 Motor 实例。
 * 初始化会立即输出低电平，使整个驱动器保持 Standby 高阻状态。
 */
typedef struct {
    GPIO_Regs *port;
    uint32_t pin;
    bool initialized;
    bool enabled;
} Motor_Standby;

bool Motor_StandbyInit(
    Motor_Standby *standby, GPIO_Regs *port, uint32_t pin);
void Motor_StandbyEnable(Motor_Standby *standby);
void Motor_StandbyDisable(Motor_Standby *standby);
bool Motor_StandbyIsEnabled(const Motor_Standby *standby);

#endif
