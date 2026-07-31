/**
  ******************************************************************************
  * @file    app_demo.c
  * @brief   四个单轴教学实验和串口命令解析
  ******************************************************************************
  * 实验1验证STEP/DIR开环动作；实验2验证A/B和PWM编码器读取；
  * 实验3自动执行闭环往返；实验4通过串口设置任意目标角度。
  ******************************************************************************
  */
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "app_demo.h"
#include "closed_loop.h"
#include "demo_config.h"
#include "encoder.h"
#include "motor.h"

/* 由5ms中断维护的软件时基，以及各实验的非阻塞状态变量。 */
static volatile uint32_t s_ms;
static char s_line[40];
static uint8_t s_line_len;
static uint32_t s_last_action;
static uint32_t s_last_output;
static uint8_t s_demo1_state;
static uint8_t s_demo3_state;
static uint8_t s_demo2_state;

/* 将闭环故障码转换成面向用户的中文说明。 */
static const char *Demo_FaultName(CL_Fault_t fault)
{
    if (fault == CL_FAULT_NONE) return "正常";
    if (fault == CL_FAULT_NO_ENCODER) return "无编码器反馈!请查编码器接线/供电";
    if (fault == CL_FAULT_DIRECTION) return "方向反了!请改AXIS_X_POSITIVE_DIR_LEVEL或发D1";
    return "驱动错误";
}

/* 打印实验4支持的全部单轴串口命令。 */
/*==========================================================================*/
/*                          实验4：串口指令控制                              */
/*==========================================================================*/
/**
  * 现象：上电后轴1保持当前位置，通过串口指令控制轴1转到指定角度，
  *       也可以查询状态、当前位置清零、立即停止、清故障和翻转正方向。
  *
  * 操作方法：
  *   1. 串口设置为115200-8-N-1，字符命令末尾必须带回车或换行；
  *   2. 发送A1 90让轴1转到+90度，发送A1 -45.5转到-45.5度；
  *   3. 发送S查询目标、实际、误差、PWM绝对角度和有符号整圈数；
  *   4. 发送Z把当前位置设为软件零点，发送X立即停止；
  *   5. 故障排除后发送C1清故障；方向相反且电机已停止时发送D1。
  *
  * 观察点：
  *   1. 目标角度是相对软件零点的多圈角度，不是PWM的0~360度绝对角度；
  *   2. 下发目标后闭环会分多段发STEP脉冲，实际角度逐渐逼近目标；
  *   3. 有故障时拒绝新目标，必须先检查编码器、方向和驱动接线。
  */
static void Demo4_PrintHelp(void)
{
    printf("\r\n================ 实验4 串口指令说明 ================\r\n");
    printf("用串口助手发送以下指令(结尾加回车换行):\r\n");
    printf("  A1 <角度>  轴1转到指定角度, 例: A1 90 或 A1 -45.5\r\n");
    printf("  S          查询轴1状态(含PWM绝对角度和有符号Z圈数)\r\n");
    printf("  Z          轴1当前位置设为零点\r\n");
    printf("  X          立即停止轴1\r\n");
    printf("  C1         清除轴1故障\r\n");
    printf("  D1         翻转轴1正方向电平(方向反了时用)\r\n");
    printf("  H          重新打印本说明\r\n");
    printf("====================================================\r\n\r\n");
}

/* 采集一次闭环、PWM绝对角度和整圈数状态并打印。 */
static void Demo4_PrintAxis(void)
{
    CL_Snapshot_t snap;
    float pwm_angle = 0.0f;
    uint8_t pwm_ok;
    CL_GetSnapshot(MOTOR_AXIS_X, &snap);
    pwm_ok = Encoder_GetPwmAngle(ENCODER_AXIS_X, &pwm_angle);
    printf("轴1: 目标=%.2f度 实际=%.2f度 误差=%.2f度 [%s] 故障:%s ",
        snap.target_angle_deg, snap.current_angle_deg,
        snap.target_angle_deg - snap.current_angle_deg,
        (snap.reached != 0U) ? "已到位" :
            ((snap.active != 0U) ? "运动中" : "空闲"),
        Demo_FaultName(snap.fault));
    if (pwm_ok != 0U) printf("PWM绝对角度=%.2f度 ", pwm_angle);
    else printf("PWM无信号 ");
    printf("Z圈数=%ld\r\n", (long)Encoder_GetZCount(ENCODER_AXIS_X));
}

/**
  * 处理一行ASCII命令。先把小写转换为大写，再依次识别角度、查询、
  * 清零、停止、清故障、翻转方向和帮助命令。
  */
static void Demo4_HandleCommand(char *line)
{
    char *p;
    float angle;
    for (p = line; *p != '\0'; p++) {
        if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
    }
    if (sscanf(line, "A1 %f", &angle) == 1) {
        if (CL_SetTargetAngle(MOTOR_AXIS_X, angle) == MOTOR_OK)
            printf("OK: 轴1 目标角度 %.2f度\r\n", angle);
        else
            printf("失败! 该轴可能有故障, 先发 C1 清除故障\r\n");
    } else if (strcmp(line, "S") == 0) {
        Demo4_PrintAxis();
    } else if (strcmp(line, "Z") == 0) {
        CL_SetZeroAll();
        printf("OK: 轴1已在当前位置清零\r\n");
    } else if (strcmp(line, "X") == 0) {
        CL_StopAll();
        printf("OK: 轴1已停止\r\n");
    } else if (strcmp(line, "C1") == 0) {
        CL_ClearFault(MOTOR_AXIS_X);
        printf("OK: 轴1故障已清除\r\n");
    } else if (strcmp(line, "D1") == 0) {
        if (CL_TogglePositiveDirLevel(MOTOR_AXIS_X) == MOTOR_OK)
            printf("OK: 轴1正方向电平已翻转\r\n");
        else
            printf("失败! 电机运动中不能翻转方向\r\n");
    } else if (strcmp(line, "H") == 0 || strcmp(line, "?") == 0) {
        Demo4_PrintHelp();
    } else if (line[0] != '\0') {
        printf("未知指令: %s (发 H 查看指令说明)\r\n", line);
    }
}

/* 实验2的单字符动作：0停止，1顺时针连续转，2逆时针连续转。 */
/*==========================================================================*/
/*                          实验2：编码器读取                                */
/*==========================================================================*/
/**
  * 现象：烧录或复位后电机默认停止；串口发送单字符即可控制轴1：
  *       1=顺时针连续旋转，2=逆时针连续旋转，0=停止。
  *       无论电机转动还是停止，串口每DEMO2_PRINT_MS打印一次编码器数据。
  *
  * 为什么不建议用手转：
  *   D36A使能后，停止STEP脉冲时电机仍然通电锁轴，不适合强行用手转动，
  *   因此由程序低速驱动电机，观察A/B计数、速度和PWM绝对角度。
  *
  * 观察点：
  *   1. 发送1和2时，A/B计数和速度的正负方向应该相反；
  *   2. A/B正交信号硬件4倍频后，电机转一整圈应变化约4000计数；
  *   3. A/B角度是相对上电零点的多圈角度，PWM角度是0~360度绝对角度；
  *   4. 编码器Z接PA25；每次Z上升沿按QEI方向累计有符号圈数；
  *   5. 如果计数方向与预期相反，修改ENCODER_AXIS_X_SIGN。
  *
  * 连续旋转的实现：
  *   TIMA1直接输出连续硬件PWM，停止命令到来时关闭定时器并强制STEP为低。
  */
static void Demo2_SetMotion(uint8_t motion)
{
    if (motion == 0U) {
        Motor_StopAll();
        s_demo2_state = 0U;
        printf("[实验2] 已停止（EN仍为高电平，驱动器继续使能/锁轴）\r\n");
    } else if (motion == s_demo2_state) {
        printf("[实验2] 已经在%s连续旋转\r\n", motion == 1U ? "顺时针" : "逆时针");
    } else {
        Motor_StartContinuous(DEMO2_FREQ_HZ, motion == 1U ? 0U : 1U);
        s_demo2_state = motion;
        printf("[实验2] 开始%s连续旋转，频率=%u Hz，发送0停止\r\n",
            motion == 1U ? "顺时针" : "逆时针", (unsigned)DEMO2_FREQ_HZ);
    }
}

/*
 * 轮询UART0接收FIFO。
 * 实验2收到单字符立即执行；实验4以回车或换行为一条命令的结束标志。
 */
static void Demo_PollUart(void)
{
    while (!DL_UART_Main_isRXFIFOEmpty(UART_0_INST)) {
        char ch = (char)DL_UART_Main_receiveData(UART_0_INST);
#if (DEMO_SELECT == 2)
        if (ch == '0') Demo2_SetMotion(0U);
        else if (ch == '1') Demo2_SetMotion(1U);
        else if (ch == '2') Demo2_SetMotion(2U);
        else if (ch != '\r' && ch != '\n' && ch != ' ' && ch != '\t')
            printf("[实验2] 未知指令 '%c'，请发送 1、2 或 0\r\n", ch);
#elif (DEMO_SELECT == 4)
        if (ch == '\r' || ch == '\n') {
            if (s_line_len != 0U) {
                s_line[s_line_len] = '\0';
                Demo4_HandleCommand(s_line);
                s_line_len = 0U;
            }
        } else if (s_line_len < sizeof(s_line) - 1U) {
            s_line[s_line_len++] = ch;
        } else {
            s_line_len = 0U;
        }
#else
        (void)ch;
#endif
    }
}

/* 初始化实验状态；模式3/4额外启用闭环，并打印当前配置和使用说明。 */
void Demo_Init(void)
{
    s_ms = 0U;
    s_last_action = 0U;
    s_last_output = 0U;
    s_demo1_state = 0U;
    s_demo2_state = 0U;
    s_demo3_state = 0U;
#if (DEMO_SELECT == 3) || (DEMO_SELECT == 4)
    CL_Init();
#endif
    printf("\r\n\r\n");
    printf("****************************************************\r\n");
    printf("*  MS42CG + D36A 步进电机闭环控制 小白例程          *\r\n");
    printf("*  主控: MSPM0G3507  串口: 115200-8-N-1             *\r\n");
    printf("****************************************************\r\n");
    printf("当前细分设置 D36A_MICROSTEP = %u (务必和拨码一致!)\r\n",
        (unsigned)D36A_MICROSTEP);
    printf("电机数量 MOTOR_COUNT = 1 (只使用轴1)\r\n");
    printf("当前运行: 实验%d ", DEMO_SELECT);
#if (DEMO_SELECT == 1)
    printf("- 开环转动实验\r\n");
    printf("电机将开始往返转动。注意: 装在机构上时请减小DEMO1_STEPS!\r\n\r\n");
#elif (DEMO_SELECT == 2)
    printf("- 编码器读取实验\r\n");
    printf("电机默认停止。发送1=顺时针连续旋转, 2=逆时针连续旋转, 0=停止。\r\n");
    printf("实验2使用单字符指令, 不需要回车换行；编码器数据会持续打印。\r\n\r\n");
#elif (DEMO_SELECT == 3)
    printf("- 闭环控制实验\r\n");
    printf("轴1将在 0 <-> ±%.1f度 间自动往返。\r\n", DEMO3_TARGET_DEG);
#if (DEMO3_OUTPUT_MODE == 1)
    printf("数据以 {B数据1:数据2:...}$ 波形格式输出, 请用配套上位机查看曲线。\r\n\r\n");
#else
    printf("\r\n");
#endif
#else
    printf("- 串口指令实验\r\n");
    Demo4_PrintHelp();
#endif
}

/* 实时5ms任务：先扩展QEI计数和PWM超时，再执行一次闭环决策。 */
void Demo_Tick5ms(void)
{
    s_ms += CL_PERIOD_MS;
    Encoder_Tick(CL_PERIOD_MS);
#if (DEMO_SELECT == 3) || (DEMO_SELECT == 4)
    CL_Process();
#endif
}

/**
  * 主循环实验调度：
  * 模式1定步正反往返；模式2周期打印编码器；模式3每4秒切换闭环目标；
  * 模式4只响应串口角度命令，闭环本身仍由5ms任务持续推进。
  */
void Demo_Process(void)
{
    Demo_PollUart();
/*==========================================================================*/
/*                          实验1：开环转动                                  */
/*==========================================================================*/
/**
  * 现象：轴1按正、反方向交替转动DEMO1_STEPS个脉冲，循环往复。
  *       电机忙时主循环不会重复启动，当前脉冲段结束后才进入下一次动作。
  *
  * 观察点：
  *   1. 修改DEMO1_FREQ_HZ可以改变STEP频率，也就是电机转速；
  *   2. 修改D36A拨码细分时必须先断电，并同步修改D36A_MICROSTEP；
  *      如果拨码和宏不一致，相同脉冲数对应的实际转角就会错误；
  *   3. 开环控制只相信已经发出的脉冲，不检查编码器位置；
  *   4. 如果堵转或负载过大造成丢步，松开后位置不会自动补回来，
  *      这就是开环控制的缺点，实验3的闭环会根据位置误差自动修正。
  *
  * 安全提示：装在机构上首次测试时应减小DEMO1_STEPS，防止撞机械限位。
  */
#if (DEMO_SELECT == 1)
    if (Motor_IsBusy(MOTOR_AXIS_X) == 0U && (s_ms - s_last_action) >= 1000U) {
        uint8_t direction = (s_demo1_state == 0U) ? 0U : 1U;
        printf(s_demo1_state == 0U ? "[实验1] 正转 %u 步, 频率 %u Hz\r\n" :
                                     "[实验1] 反转 %u 步, 频率 %u Hz\r\n",
            (unsigned)DEMO1_STEPS, (unsigned)DEMO1_FREQ_HZ);
        (void)Motor_SetDirection(MOTOR_AXIS_X, direction);
        (void)Motor_Start(MOTOR_AXIS_X, DEMO1_STEPS, DEMO1_FREQ_HZ);
        s_demo1_state = (uint8_t)!s_demo1_state;
        s_last_action = s_ms;
    }
#elif (DEMO_SELECT == 2)
    if ((s_ms - s_last_output) >= DEMO2_PRINT_MS) {
        float pwm_angle = 0.0f;
        uint8_t pwm_ok = Encoder_GetPwmAngle(ENCODER_AXIS_X, &pwm_angle);
        printf("状态=%s | 轴1: AB计数=%ld 角度=%.2f度 速度=%.1f度/秒 ",
            s_demo2_state == 1U ? "顺时针" :
                (s_demo2_state == 2U ? "逆时针" : "停止"),
            (long)Encoder_GetCount(ENCODER_AXIS_X),
            Encoder_GetAngle(ENCODER_AXIS_X),
            Encoder_CalcSpeedDps(ENCODER_AXIS_X, DEMO2_PRINT_MS));
        if (pwm_ok != 0U) printf("PWM绝对角度=%.2f度 ", pwm_angle);
        else printf("PWM无信号 ");
        printf("Z圈数=%ld\r\n\r\n", (long)Encoder_GetZCount(ENCODER_AXIS_X));
        s_last_output = s_ms;
    }
/*==========================================================================*/
/*                          实验3：闭环控制                                  */
/*==========================================================================*/
/**
  * 现象：轴1每隔DEMO3_SWITCH_MS切换一次目标，按照
  *       +DEMO3_TARGET_DEG -> 0 -> -DEMO3_TARGET_DEG -> 0的顺序循环往返。
  *
  * 观察点：
  *   1. 文本模式下可以看到误差逐渐变小，最后进入CL_TOLERANCE_COUNTS容差；
  *   2. DEMO3_OUTPUT_MODE=1时输出{B目标:实际:误差}$，可用上位机画曲线；
  *   3. 在安全范围内轻微阻挡或扰动电机，松开后闭环会继续补偿到目标；
  *   4. 如果电机转动而编码器不动，会报无反馈故障；
  *      如果编码器明显朝相反方向变化，会报方向故障；
  *   5. 第一次带机构测试建议把DEMO3_TARGET_DEG设为5度，并确认没有限位干涉。
  *
  * 控制过程：每5ms读取A/B位置；当前短脉冲段结束后重新计算误差，
  *           再选择方向、步数和频率发送下一段，直到稳定到位。
  */
#elif (DEMO_SELECT == 3)
    if ((s_ms - s_last_action) >= DEMO3_SWITCH_MS) {
        float target;
        if (CL_GetFault(MOTOR_AXIS_X) != CL_FAULT_NONE)
            printf("[实验3] 轴1故障: %s\r\n", Demo_FaultName(CL_GetFault(MOTOR_AXIS_X)));
        if (s_demo3_state == 0U) target = DEMO3_TARGET_DEG;
        else if (s_demo3_state == 1U) target = 0.0f;
        else if (s_demo3_state == 2U) target = -DEMO3_TARGET_DEG;
        else target = 0.0f;
        (void)CL_SetTargetAngle(MOTOR_AXIS_X, target);
        s_demo3_state = (uint8_t)((s_demo3_state + 1U) % 4U);
        s_last_action = s_ms;
    }
    if ((s_ms - s_last_output) >= DEMO3_OUTPUT_MS) {
        CL_Snapshot_t snap;
        CL_GetSnapshot(MOTOR_AXIS_X, &snap);
#if (DEMO3_OUTPUT_MODE == 1)
        printf("{B%.2f:%.2f:%.2f}$", snap.target_angle_deg,
            snap.current_angle_deg, snap.target_angle_deg - snap.current_angle_deg);
#else
        static uint8_t s_div;
        if (++s_div >= 10U) {
            s_div = 0U;
            printf("轴1: 目标=%7.2f 实际=%7.2f 误差=%6.2f %s\r\n",
                snap.target_angle_deg, snap.current_angle_deg,
                snap.target_angle_deg - snap.current_angle_deg,
                snap.reached != 0U ? "已到位" : "运动中");
        }
#endif
        s_last_output = s_ms;
    }
#endif
}
