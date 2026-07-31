#ifndef APPLICATION_CONFIG_H
#define APPLICATION_CONFIG_H

#include <stdbool.h>

/*
 * 这一文件集中保存必须上车实测的参数。烧录后先保持车轮离地，用 t 单次查看
 * 完整遥测，或用 v 开关五路原始值连续输出，再修正黑线/背景数组、电机反相
 * 和差速方向；确认后才发送 s。
 *
 * 五路模拟信号进入 MSPM0 前必须确认始终不超过 3.3 V。扩展板接口旁的
 * +5 V 不能证明信号电平对 ADC 安全。
 */

#define APPLICATION_LEFT_MOTOR_INVERTED (false)
#define APPLICATION_RIGHT_MOTOR_INVERTED (false)
#define APPLICATION_MOTOR_MAXIMUM_OUTPUT (350U)
#define APPLICATION_MOTOR_REVERSAL_DELAY_MS (5U)

#define APPLICATION_DEFAULT_DRIVE_OUTPUT (180)
#define APPLICATION_MAXIMUM_TURN_OUTPUT (90)
#define APPLICATION_LEFT_OPEN_LOOP_SCALE (1000U)
#define APPLICATION_RIGHT_OPEN_LOOP_SCALE (1000U)

/*
 * 连续原始值输出默认关闭，收到 v 后每 100 ms 输出一行。115200 波特率下，
 * 五路 12 位数值的单行发送时间不超过约 2.7 ms，不会占满 10 ms 控制周期。
 */
#define APPLICATION_RAW_STREAM_PERIOD_MS (100U)

/*
 * 完整控制快照比单独五路 raw 更长，因此使用 200 ms 周期。输出默认关闭，
 * 收到 d 后开启；它与 v 原始值流互斥，避免两个连续输出同时占用串口。
 */
#define APPLICATION_DEBUG_STREAM_PERIOD_MS (200U)

/*
 * 上电后每秒主动报告一次 UART 接收计数。这个诊断不依赖接收命令：
 * 若发送字符后 rxBytes 仍为 0，说明字节没有到达 UART0 RX/PA11；
 * 若 rxBytes 增加，则可继续检查命令队列和解析逻辑。
 */
#define APPLICATION_UART_DIAGNOSTIC_PERIOD_MS (1000U)

/*
 * 下列数值只是安全启动用的初始标定，必须用本车五路探头在背景和黑线上分别
 * 实测后替换。每个数组从左至右对应 L2、L1、C、R1、R2。
 */
#define APPLICATION_LINE_CHANNEL_MAP \
    {0U, 1U, 2U, 3U, 4U}
#define APPLICATION_LINE_BACKGROUND_VALUES \
    {270U, 270U, 270U, 270U, 270U}
#define APPLICATION_LINE_VALUES \
    {500U, 40U, 30U, 25U, 500U}
#define APPLICATION_LINE_POSITION_WEIGHTS \
    {-2000, -1000, 0, 1000, 2000}
#define APPLICATION_LINE_MINIMUM_CALIBRATION_RANGE (100U)
#define APPLICATION_LINE_MINIMUM_TOTAL_STRENGTH (300U)

#define APPLICATION_TURN_KP (0.65f)
#define APPLICATION_TURN_KI (0.0f)
#define APPLICATION_TURN_KD (0.0f)
#define APPLICATION_TURN_INVERTED (false)
#define APPLICATION_DERIVATIVE_FILTER_COEFFICIENT (0.20f)
#define APPLICATION_MAXIMUM_INVALID_FRAMES (3U)

#endif
