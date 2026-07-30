下面这份可以直接保存为仓库根目录的 `GPT_LOG.md`。它不是开发流水账，而是**项目上下文、已定决策和后续协作规则的交接文档**，新对话先读取它即可继续。



````markdown
# GPT_LOG 最新状态覆盖

> 更新时间：2026-07-30
>
> 本节优先级高于本文件中更早的硬件、底盘和巡迹方案。
> 旧日志中涉及“八路灰度传感器”“阿克曼前轮舵机巡迹”
> “ADC0 八通道序列”的内容已经失效。

---

## 1. 唯一有效工作区

当前 GitHub 仓库存在多条并行开发线。

GPT、Codex 处理本方案时只允许读取和修改：

```text
ClBrIsWorks/
```

其他目录：

```text
TianMengXing_74HC4067/
VisionTransmit/
```

约束：

* `TianMengXing_74HC4067` 是队友独立开发的巡迹版本；
* `VisionTransmit` 是独立图传项目；
* 两者都不得作为 `ClBrIsWorks` 的代码依赖；
* 不从两者复制接口、SysConfig 或应用逻辑；
* 后续提到“当前代码”“正式工程”“现有接口”，默认均指
  `ClBrIsWorks/`；
* Codex 只在 `ClBrIsWorks/` 内实施修改。

---

## 2. 传感器规则变化

队伍当前按照老师对题目规则的解释执行：

```text
允许红外传感器
不采用灰度传感器
```

现有红外巡迹模块具有：

```text
5 路模拟输出
```

因此废弃原来的八路传感器方案。

巡迹仍然使用模拟量，不退化成五路数字开关：

```text
ADC 原始值
→ 每路独立黑白标定
→ 归一化到 0～1000
→ 五路加权质心
→ 连续线位置
```

---

## 3. 五路红外最终引脚

按传感器物理位置从左到右：

```text
最左 L2：PA15 / ADC1_A0
左侧 L1：PA17 / ADC1_A2
中央 C ：PA18 / ADC1_A3
右侧 R1：PB18 / ADC1_A5
最右 R2：PA21 / ADC1_A7
```

全部使用：

```text
ADC1 单实例五通道序列
```

ADC 序列结果顺序：

```c
raw[0] = PA15;  /* 最左 */
raw[1] = PA17;
raw[2] = PA18;  /* 中央 */
raw[3] = PB18;
raw[4] = PA21;  /* 最右 */
```

当前默认配置：

```c
channelMap = {0U, 1U, 2U, 3U, 4U};

positionWeight = {
    -2000,
    -1000,
        0,
     1000,
     2000
};
```

SysConfig ADC1 序列：

```text
MEM0：ADC1_A0 / PA15
MEM1：ADC1_A2 / PA17
MEM2：ADC1_A3 / PA18
MEM3：ADC1_A5 / PB18
MEM4：ADC1_A7 / PA21
```

约束：

* 不再实现 ADC0 与 ADC1 双序列合并；
* 保留旧 `ADC_ReadSequence8()`，不破坏历史接口；
* 新增或泛化五路序列读取接口；
* `LINE_SENSOR_COUNT` 改为 5；
* 黑白标定值和总强度阈值必须重新实测；
* 红外模拟输出进入 MSPM0 前必须确认不超过 3.3V。

---

## 4. 底盘结构变化

废弃当前巡迹运行链中的阿克曼转向。

机械结构改为：

```text
左后轮：独立直流电机
右后轮：独立直流电机
前部：万向轮
转向：左右轮差速
```

原阿克曼前轮和转向舵机从底盘拆除。

`servo` 模块源码与 PB14/TIMG12 配置继续保留，但不再参与巡迹底盘
运行链；该资源留给 H 题摆杆执行器。

---

## 5. 差速底盘控制约定

巡迹控制器输出不再叫舵机转向量，而应表示：

```text
turnOutput
```

正方向约定：

```text
turnOutput > 0：左转
turnOutput < 0：右转
```

差速混合：

```c
leftOutput  = driveOutput - turnOutput;
rightOutput = driveOutput + turnOutput;
```

`chassis` 是通用有符号差速层，允许左右轮正反转。混合结果超过
`maximumDriveOutput` 时，两侧按相同比例缩放，保留转弯曲率。

当前巡迹的“不倒转”属于 Application 策略：

```c
turnOutput = Clamp(
    turnOutput,
    -Abs(driveOutput),
     Abs(driveOutput));
```

因此巡迹运行时只会出现两轮前进，或者一侧停止、另一侧前进；原地转向和
倒车能力仍保留在通用 chassis 中。

---

## 6. 当前实施状态

已经完成：

```text
ADC_ReadSequence5()
五路 line_sensor
line_control 的 turnOutput 语义
有符号差速 chassis 与等比例饱和
Application 巡迹前进限制
有 STBY / 无 STBY 兼容
五路、PID、Motor、Chassis、Application 策略主机测试
```

保持通用且不写死硬件实例：

```text
motor
pid
system_time
HC-05 / UART（使用 HC05_UART_INST 生成宏）
MPU6050
```

不进入当前巡迹底盘运行链但保留：

```text
servo 与 PB14/TIMG12
编码器 GPIOA 资源
OLED 软件开漏 I2C 资源
```

---

## 7. 差速 Chassis 推荐接口

当前接口：

```c
void Chassis_SetWheelOutputs(
    Chassis *chassis,
    int16_t leftOutput,
    int16_t rightOutput,
    uint32_t nowMs);

void Chassis_SetDriveTurn(
    Chassis *chassis,
    int16_t driveOutput,
    int16_t turnOutput,
    uint32_t nowMs);
```

保留：

```c
void Chassis_Brake(
    Chassis *chassis,
    uint32_t nowMs);

void Chassis_Coast(
    Chassis *chassis,
    uint32_t nowMs);

void Chassis_Process(
    Chassis *chassis,
    uint32_t nowMs);
```

`Chassis_Process()` 仍须在每次主循环执行，以推进电机非阻塞换向状态。

---

## 8. 五路 LineSensor 约束

`line_sensor` 仍保持纯数据模块：

```c
bool LineSensor_ProcessRaw(
    LineSensor *sensor,
    const uint16_t adcRaw[5]);
```

不允许：

* 直接读取 ADC；
* 绑定 SysConfig 实例；
* 保存 ADC 错误次数；
* 操作电机。

无效帧仍须：

```text
valid = false
position 保留上一帧有效位置
```

总强度阈值不能从八路版本按比例猜测，必须根据五路模块重新标定。

---

## 9. 巡迹运行链

最新控制链：

```text
ADC1 五通道序列
→ 五路红外独立标定
→ 加权质心线位置
→ P / PD 控制
→ turnOutput
→ 差速混合
→ 左右 Motor_SetOutput()
```

故障链：

```text
ADC 错误
或连续丢线
→ 左右电机短路制动
→ 进入错误状态
```

不再执行舵机回中。

---

## 10. 扩展板关键映射

电机整路绑定：

```text
右轮：PA12 / TIMG0_C0 + AIN1 / PA16 + AIN2 / PB24
左轮：PA13 / TIMG0_C1 + BIN1 / PB17 + BIN2 / PB19
```

TB6612 STBY 在扩展板上固定使能，因此：

```text
Chassis_Config.standby 允许为 NULL
```

不能继续把 PA7 当作 STBY，PA7 在扩展板上连接蜂鸣器。

HC-05 / 调试串口：

```text
UART1_TX：PB6
UART1_RX：PB7
代码只使用 HC05_UART_INST 生成宏
```

UART2 的 PA23/PA24 接线曾在 115200、8-N-1 下持续收到乱码，已停止作为
当前调试串口使用。生成代码仍采用 32 MHz BUSCLK、16 倍过采样，业务代码
没有 UART 实例字面量。

调试命令：

```text
t：单次输出完整遥测，包括五路 raw、strength、线位置和差速输出
v：开关五路 raw 连续输出，周期 100 ms，默认关闭
```

MPU6050：

```text
I2C0_SDA：PA0
I2C0_SCL：PA1
```

编码器资源：

```text
左编码器 A：PA14
左编码器 B：PA25
右编码器 A：PA26
右编码器 B：PA27
```

四路均属于 GPIOA，共用 `GROUP1_IRQHandler()`。编码器模块尚未实现；
实现时必须使用 `GPIO_ENCODER_*` 生成宏，并实测核对 A/B 顺序和计数方向。

OLED 软件 I2C：

```text
SCL：PA31
SDA：PA28
```

高电平必须通过输入/高阻释放，低电平才切换为输出并拉低，禁止推挽输出高。

---

## 11. 下一阶段优先级

五路开环巡迹跑通后：

```text
编码器 x4 解码
→ PPS
→ 输出轴每圈计数标定
→ 左右轮速度 PI
→ 差速目标轮速
→ 巡迹控制输出目标速度差
```

差速底盘中，左右轮速度本身决定转向，因此编码器闭环的优先级高于原阿克曼方案。

---

## 12. 小球水管执行器预案

主控仍为：

```text
MSPM0G3507 / 天猛星
```

推荐架构：

```text
天猛星：
    巡迹
    任务状态机
    水管外环位置控制
    发送目标位置/速度

外部闭环电机驱动器：
    电流环
    速度环
    编码器闭环
    功率驱动
```

优先使用具有内部闭环的：

```text
闭环步进驱动器
或
集成 FOC 的无刷伺服驱动器
```

天猛星只发送：

```text
STEP/DIR
UART
CAN
或目标 PWM
```

不建议在比赛期间把三相逆变、电流采样和完整 FOC 内环直接并入巡迹主固件。

最终接口和引脚必须等电机及驱动器型号确定后再锁定，不允许提前猜测。

````



````markdown
# GPT 项目上下文日志

## 2026-07-30 当前决策覆盖

本节覆盖下方 2026-07-29 的阿克曼八路巡迹规划；下方内容保留为历史背景。

当前机械与巡迹链已经改为：

```text
五路模拟红外传感器
→ ADC1 单实例 MEM0～MEM4 序列
→ 五路归一化和加权质心
→ line_control 输出 turn
→ 左右后轮差速混合
→ 前部万向轮
```

五路物理顺序和引脚固定为：

```text
L2  PA15 / ADC1_A0 / MEM0
L1  PA17 / ADC1_A2 / MEM1
C   PA18 / ADC1_A3 / MEM2
R1  PB18 / ADC1_A5 / MEM3
R2  PA21 / ADC1_A7 / MEM4
```

`PB14/TIMG12` 和 `servo` 模块保留给摆杆或其他 PWM 执行器，但不进入
巡迹底盘运行链；PA22 也不用于巡迹。当前差速混合禁止车轮反转：

```text
left  = clamp(drive - turn, 0, maximumDriveOutput)
right = clamp(drive + turn, 0, maximumDriveOutput)
```

红外模拟输出接入前必须确认不超过 3.3 V。扩展板接口旁存在 +5 V
不能作为模拟信号对 MSPM0 ADC 安全的依据。

> 最后更新：2026-07-30  
> 项目：全国大学生电子设计竞赛 H 题  
> 当前目标：两轮差速小车，先完成五路模拟红外巡迹，后续加入摆杆控制  
> 仓库：`rainbowbloodyrainbow/ForTICup`

---

## 0. 本文件用途

本文件用于在 GPT、Codex 或新的对话之间传递完整项目上下文。

新的模型接手时应：

1. 先阅读本文件；
2. 再读取仓库当前代码和最近提交；
3. 以仓库实际代码为准；
4. 不重复询问本文已经确认的信息；
5. 不擅自把阿克曼底盘重新设计成差速转向底盘；
6. 不重新进行已经完成的架构争论；
7. 修改代码前先确认现有接口，避免创建重复模块。

本文件记录的是已经达成一致的设计约束，不代表所有模块已经实现。

---

# 1. 当前赛题与开发阶段

正式赛题已经公布。

当前选择：

```text
H 题
小车巡迹 + 摆杆控制
````

底盘不是常见的两轮差速转向车，而是：

```text
前轮：阿克曼联动转向，由一个舵机控制
后轮：左右两个独立驱动轮
左右后轮各有一个电机和一个 AB 相编码器
```

目前是比赛第一天。

当前阶段只完成最简单的八路光电巡迹：

```text
ADC0 八通道序列
→ 八路独立黑白标定
→ 归一化
→ 加权质心计算线位置
→ P / PD 转向控制
→ 舵机转向
→ 两个后轮低速开环驱动
```

今天不接入运行链：

```text
编码器
轮速闭环
里程计
MPU6050 航向
摆杆控制
06_motion
```

但不能删除这些资源或破坏后续扩展接口。

---

# 2. 仓库与工作区状态

本地正式工作区：

```text
/mnt/32FA76A4FA7663CF/study/ForTICup
```

目标是只保留一个正式固件工程：

```text
根目录唯一 main.c
根目录唯一 Makefile
唯一 all.syscfg
唯一一套 SysConfig 生成文件
```

`Examples/` 可以保留，但不参与正式工程构建。

已知本地存在、但远端未必已经提交的内容：

```text
all.syscfg
myownlib/01_platform/
myownlib/02_device/motor/
tests/host/motor/
```

远端仓库可能滞后，因此任何模型开始工作时必须优先读取本地当前状态或最新提交，不能只依赖旧的 GitHub 页面。

不要在仓库、日志或对话中保存访问令牌、密码等秘密信息。

---

# 3. 总体软件分层

当前达成一致的分层：

```text
00_generated/
    SysConfig 自动生成代码

01_platform/
    MCU 外设和基础平台接口

02_device/
    具体硬件设备

03_algorithm/
    与外设无关的纯算法

04_control/
    闭环控制器和控制策略

05_robot/
    整车级对象和底盘协调

06_motion/
    通用动作，例如直行、转角、圆弧

07_application/
    具体赛题流程和状态机
```

建议目录：

```text
MSPM0/
├── Makefile
├── main.c
├── all.syscfg
│
├── 00_generated/
│
├── myownlib/
│   ├── 01_platform/
│   │   ├── output/
│   │   ├── adc/
│   │   ├── uart 或 hc05/
│   │   └── system_time/
│   │
│   ├── 02_device/
│   │   ├── motor/
│   │   ├── encoder/
│   │   ├── mpu6050/
│   │   ├── line_sensor/
│   │   └── servo/
│   │
│   ├── 03_algorithm/
│   │   ├── pid/
│   │   └── filter/
│   │
│   ├── 04_control/
│   │   ├── wheel_control/
│   │   ├── heading_control/
│   │   └── line_control/
│   │
│   ├── 05_robot/
│   │   ├── chassis/
│   │   └── odometry/
│   │
│   ├── 06_motion/
│   │   └── motion/
│   │
│   └── 07_application/
│       └── application/
│
├── Examples/
└── tests/
    └── host/
```

分层是依赖方向，不代表每次调用必须逐层穿过所有目录。

`03_algorithm` 属于横向公共算法库，可被 `04_control`、`05_robot` 和 `06_motion` 使用。

---

# 4. 模块边界共识

## 4.1 `output`

属于 `01_platform`。

负责：

```text
数字输出
PWM 基础操作
PWM 千分比占空比
PWM 有效高电平 ticks
PWM 微秒脉宽
```

它不知道电机、舵机、车轮、循迹等概念。

---

## 4.2 `adc`

属于 `01_platform`。

负责：

```text
单通道 ADC 读取
ADC 八通道序列读取
启动转换
有限时间等待
读取 ADCMEM
返回状态
```

它不保存错误计数，不理解八路光电传感器物理顺序。

---

## 4.3 `motor`

属于 `02_device`。

定义为真实的电机执行器，负责：

```text
怎样驱动电机
PWM
方向
短路制动
高阻滑行
STBY
安装方向反相
输出限幅
非阻塞换向保护
```

不负责：

```text
编码器测速
目标轮速
PID
车轮速度闭环
车体运动
走多少距离
```

当前现有实现优先复用，不重新设计。

---

## 4.4 `encoder`

属于 `02_device`。

负责：

```text
AB 相 x4 正交解码
累计计数
区间增量
PPS
方向反转
非法状态跳变统计
计数回绕安全
初始 AB 状态同步
```

不包含轮径。

配置字段避免模糊的 `PPR`、`CPR`，后续使用：

```c
countsPerOutputRevolution
```

含义是：

```text
采用当前解码倍率后，
车轮输出轴完整旋转一圈产生的最终计数。
```

---

## 4.5 `line_sensor`

属于 `02_device`，但实现为纯数据处理。

负责：

```text
八路通道映射
每路独立黑白标定
归一化到 0～1000
位置权重
加权质心
总强度
结果有效性
保留上一次有效位置
```

不得：

```text
直接调用 ADC
绑定 LINE_ADC_INST
引用 DriverLib
保存 ADC 错误次数
```

应用层负责先读取 ADC，再把数组交给 `LineSensor_ProcessRaw()`。

---

## 4.6 `servo`

属于 `02_device`。

负责：

```text
归一化转向命令
脉宽限制
中位
左右安全极限
方向反相
PWM 启用和禁用
```

不理解循迹误差。

必须明确区分：

```c
Servo_Center();   // 保持 PWM，舵机回到中位
Servo_Disable();  // 停止 PWM，舵机通常失去保持力
```

丢线或故障时使用 `Servo_Center()`，不是 `Servo_Disable()`。

---

## 4.7 `pid`

属于 `03_algorithm`。

纯数学模块，不包含任何硬件头文件。

负责：

```text
P
I
D
显式 dt
积分限幅
输出限幅
微分滤波
状态复位
```

今天巡迹先使用：

```text
Kp > 0
Ki = 0
Kd = 0
```

确认 P 方向和基础跟随后，再加少量 D。

---

## 4.8 `line_control`

属于 `04_control`。

负责：

```text
线位置误差
→ PID
→ 舵机归一化转向命令
```

还负责短暂无效帧容忍：

```text
TRACKING
HOLDING
LINE_LOST
```

不读取 ADC，不直接操作舵机。

---

## 4.9 `wheel_control`

属于 `04_control`，后续实现。

负责：

```text
编码器 PPS
m/s 换算
目标轮速
前馈
PI / PID
死区补偿
Motor_SetOutput()
```

接口的目标和测量速度最终使用：

```text
m/s
```

同时可提供 PPS 诊断值。

---

## 4.10 `chassis`

属于 `05_robot`。

当前第一阶段负责组合：

```text
左后轮电机
右后轮电机
公共 STBY
前轮转向舵机
```

当前开环接口：

```text
统一驱动输出
转向命令
左右开环修正
制动
滑行
舵机回中
电机换向状态推进
```

后续会扩展为阿克曼底盘接口：

```text
速度
曲率
实际前轮转角
左右后轮速度分配
```

不能用差速转向模型替代阿克曼模型。

---

## 4.11 `odometry`

属于 `05_robot`，后续实现。

阿克曼里程计应使用：

```text
左右后轮编码器增量
实际前轮转角
轮距
轴距
轮径
```

不能按普通两轮差速车公式直接计算航向。

---

## 4.12 `motion`

属于 `06_motion`，后续实现。

负责通用动作：

```text
直行指定距离
转到指定角度
沿圆弧运动
停车
接近目标
```

不负责具体赛题完整流程。

---

## 4.13 `application`

属于 `07_application`。

负责：

```text
ADC 采样
模块组合
HC-05 命令
状态机
安全策略
控制节拍
低频遥测
具体赛题流程
```

应用层是唯一知道“当前在完成 H 题”的位置。

---

# 5. 主控与开发环境

```text
MCU：MSPM0G3507
开发板：立创·天猛星
封装：LQFP-64(PM)
SDK：MSPM0 SDK 2.11.00.07
SysConfig：1.28.0
当前 CPU 时钟：32 MHz
系统时基：SysTick 1 ms
```

当前 `all.syscfg` 已通过 SysConfig CLI 校验。

不要硬编码定时器输入频率，应优先使用 SysConfig 生成的频率宏。

---

# 6. 已确认的 SysConfig 资源

## 6.1 TB6612 电机 PWM

```text
左 PWMA：
    PA12
    TIMG0_C0

右 PWMB：
    PA13
    TIMG0_C1

PWM：
    20 kHz
    边沿对齐
    非反相
    定时器上电停止
    初始占空比 0
```

---

## 6.2 TB6612 方向与使能

```text
AIN1：PB10
AIN2：PB11

BIN1：PB12
BIN2：PB13

STBY：PA7
```

所有控制 GPIO 上电为低。

STBY：

```text
高电平：使能
低电平：Standby，高阻
```

现有真实接口：

```c
Motor_StandbyEnable(...);
Motor_StandbyDisable(...);
```

不要发明不存在的：

```c
Motor_SetStandby(false);
```

---

## 6.3 转向舵机

```text
引脚：PB14
定时器：TIMG12_C1
频率：50 Hz
定时器上电停止
初始占空比 0
```

舵机使用 SysConfig 生成的真实时钟频率：

```c
SERVO_PWM_INST_CLK_FREQ
```

不能假设永远等于 CPU 主频。

---

## 6.4 编码器

左编码器：

```text
A：PB2
B：PB3
```

右编码器：

```text
A：PB4
B：PB5
```

配置：

```text
GPIOB 输入
四个引脚上升沿和下降沿中断
共用 GPIOB_INT_IRQn
SysConfig IRQ 宏：
GPIO_ENCODER_INT_IRQN
```

---

## 6.5 八路光电 ADC

```text
PA27 / ADC0_A0
PA26 / ADC0_A1
PA25 / ADC0_A2
PA24 / ADC0_A3
PB25 / ADC0_A4
PB24 / ADC0_A5
PB20 / ADC0_A6
PA22 / ADC0_A7
```

配置：

```text
软件触发
单次八通道序列
12 位
当前采样时间：每通道 125 μs
```

八通道总采样时间约 1 ms。

---

## 6.6 MPU6050

```text
I2C0
SDA：PA0
SCL：PA1
400 kHz
非阻塞 I2C 中断状态机
```

已完成：

```text
100 Hz
±250 dps
±2 g
14 字节读取
开发板实测通过
```

Z 轴零偏校准、航向积分和轴映射属于更高层。

MPU6050 在车上的最终安装方向尚未确认。

---

## 6.7 HC-05

```text
UART1
TX：PA8
RX：PA9
115200
8N1
RX 中断
```

---

# 7. 只预留、不初始化的资源

以下资源当前只预留引脚，不创建活动驱动或 SysConfig 外设实例，也不允许其他模块占用。

## 串口屏

```text
UART2_TX：PB15
UART2_RX：PB16
```

## NRF24L01

```text
SPI0_MOSI：PB17
SPI0_SCK：PB18
SPI0_MISO：PB19
CSN：PB23
CE：PB26
IRQ：PB27
```

## HC-SR04

```text
TRIG：PA28
ECHO：PA31
```

其他：

```text
PA15 当前空闲
TB6612 模块没有 FAULT 引脚
```

---

# 8. 电机和 TB6612 语义

电机：

```text
左右各一只普通 310 有刷直流电机
左右分别驱动两个后轮
```

驱动器：

```text
TB6612FNG 双路 H 桥
```

控制：

```text
A 通道：AIN1 + AIN2 + PWMA
B 通道：BIN1 + BIN2 + PWMB
公共 STBY
```

真值语义：

```text
正向：
    IN1=1
    IN2=0
    PWM 输出占空比

反向：
    IN1=0
    IN2=1
    PWM 输出占空比

短路制动：
    PWM=0
    IN1=1
    IN2=1

高阻滑行：
    IN1=0
    IN2=0
    PWM=1

Standby 高阻：
    STBY=0
```

现有 `motor` 已实现：

```text
-1000～1000 有符号输出
最大输出限幅
安装方向 inverted
Motor_Brake()
Motor_Coast()
非阻塞换向等待
公共 STBY 控制
```

真实调用约束：

```text
Motor_SetOutput() 需要 nowMs
Motor_Brake() 需要 nowMs
Motor_Process() 需要持续调用
```

不要增加含义不清楚的：

```c
Motor_Stop();
```

停车策略：

```text
普通受控停车：
    减速到 0
    短路制动

立即中止：
    短路制动

明确要求自由滑行：
    Motor_Coast()

彻底关闭驱动器：
    STBY 拉低
```

注意：

```text
STBY=0 是高阻，不是主动制动。
```

---

# 9. 底盘机械结构

```text
结构：阿克曼转向

前轮：
    两个联动转向轮
    一个舵机控制

后轮：
    两个独立驱动轮
    左右各一个 310 电机
    左右各一个编码器

不使用万向轮
```

普通尺测得近似尺寸：

```text
前轮外径：约 48 mm
后轮外径：约 68 mm
后轮宽度：约 27 mm
后轮轮距：约 168 mm
前后轴距：约 124 mm
```

这些不是精密标定值。

必须放在配置中：

```text
Chassis_Config
Odometry_Config
```

不能作为库内固定常量。

未知且不得猜测：

```text
电机减速比
额定电压
空载转速
最高车速
```

---

# 10. 编码器锁定设计

已确定：

```text
左右各一个独立 AB 两相编码器
四个 GPIO 双边沿中断
x4 正交状态表解码
```

ISR 必须：

1. 一次读取 GPIOB 当前状态；
2. 分别提取左右 AB 两位；
3. 使用 `previousAB -> currentAB` 的 16 项状态表；
4. 得到 `+1 / -1 / 0`；
5. 记录非法跨状态跳变；
6. 只更新整数计数。

ISR 禁止：

```text
浮点
PID
回调
串口输出
复杂状态机
```

GPIOB NVIC 必须在：

```text
读取四个编码器引脚
→ 建立初始 AB 状态
```

之后再开启，避免启动虚假计数。

未知参数：

```text
霍尔、光电或磁编码器
编码器位于电机轴还是输出轴
原始 PPR
减速比
输出轴最终 x4 计数
推挽、开漏或 5V 输出
最高脉冲频率
```

这些未知不影响接口和状态表实现，只影响配置和输入电气设置。

---

# 11. 时间与控制调度

控制周期：

```text
10 ms
100 Hz
```

控制计算：

```text
主循环执行
```

ISR：

```text
只更新时间和发布控制周期序号
```

MPU6050：

```text
100 Hz 非阻塞更新
```

HC-05 遥测：

```text
默认约 100 ms 一次
```

不使用简单的：

```c
bool controlPending;
```

因为它无法统计漏掉的周期。

使用单调递增控制序号：

```c
volatile uint32_t gControlSequence;
```

`system_time` 的公开接口锁定为：

```c
void SystemTime_On1msTick(void);

uint32_t SystemTime_GetMs(void);

uint32_t SystemTime_GetControlSequence(void);

uint32_t SystemTime_ElapsedMs(
    uint32_t now,
    uint32_t previous);
```

`SystemTime_On1msTick()` 内部完成：

```text
1 ms 累计
10 ms 分频
控制序号递增
```

主循环发现漏周期时：

```text
统计 missedControlCycles
只使用最新数据运行一次控制步骤
不补跑多个使用旧数据的控制步骤
```

`SystemTime_ElapsedMs()` 使用无符号减法：

```c
return now - previous;
```

天然支持 `uint32_t` 回绕。

`Chassis_Process()` 必须每次主循环调用，不能只在 10 ms 控制周期调用，因为电机换向状态需要及时推进。

---

# 12. 速度与方向约定

统一单位：

```text
encoder：
    原始累计计数
    区间增量
    PPS

wheel_control：
    目标速度 m/s
    测量速度 m/s
    额外输出 PPS 诊断

chassis：
    线速度 m/s
    曲率 1/m
    舵角 rad
    航向 rad
```

统一正方向：

```text
车辆前进：
    线速度为正

左后轮向前：
    轮速为正

右后轮向前：
    轮速为正

俯视逆时针：
    航向角和角速度为正

向左转向：
    前轮转角和曲率为正

电机输出：
    -1000～1000
```

安装差异只能在配置中处理：

```text
Motor_Config.inverted
Encoder_Config.inverted
Servo_Config.inverted
LineControl_Config.steeringInverted
```

上层禁止散布临时负号修正。

---

# 13. 今天锁定的实现范围

今天实施：

```text
扩展 adc
扩展 output
建立 system_time
建立 line_sensor
建立 servo
建立 pid
建立 line_control
建立 chassis
建立 application
建立 main.c
建立根 Makefile
生成唯一 00_generated
```

今天不实施：

```text
encoder 运行链
wheel_control
odometry
heading_control
MPU6050 航向融合
摆杆控制
06_motion
```

现有 `motor` 直接复用。

---

# 14. `adc` 实施规格

保留现有单通道接口。

新增：

```c
#define ADC_SEQUENCE8_COUNT 8U

typedef enum {
    ADC_STATUS_OK = 0,
    ADC_STATUS_INVALID_ARGUMENT,
    ADC_STATUS_TIMEOUT
} ADC_Status;

ADC_Status ADC_ReadSequence8(
    ADC12_Regs *adc,
    uint16_t result[ADC_SEQUENCE8_COUNT]);
```

行为：

```text
验证参数
清除上一次序列状态
软件触发转换
有限次数等待最后一个通道完成
读取 ADCMEM0～ADCMEM7
返回状态
```

禁止无限等待。

ADC 模块不保存：

```text
错误次数
最近结果
物理通道顺序
传感器类型
```

ADC 错误计数只放在 `Application`。

---

# 15. `output` PWM 实施规格

保留原有千分比接口。

新增：

```c
bool PwmOutput_SetHighTicks(
    GPTIMER_Regs *timer,
    DL_TIMER_CC_INDEX channel,
    uint32_t highTicks);

bool PwmOutput_SetPulseUs(
    GPTIMER_Regs *timer,
    DL_TIMER_CC_INDEX channel,
    uint32_t timerClockHz,
    uint32_t pulseUs);
```

命名必须和参数一致。

当前向下计数、边沿对齐、非反相 PWM 下：

```c
compareValue = periodTicks - highTicks;
```

但实现必须根据现有 `PwmOutput_SetDuty()` 和 DriverLib 确认：

```text
LOAD
LOAD + 1
compare 边界
0% 和 100% 特殊值
```

不能机械照搬公式。

`SetPulseUs()` 必须检查：

```text
timer != NULL
timerClockHz != 0
uint64_t 乘法不溢出
换算结果可放入 uint32_t
highTicks <= 实际 PWM 周期
```

换算：

```c
uint64_t ticks64 =
    ((uint64_t)timerClockHz * pulseUs)
    / 1000000ULL;
```

---

# 16. `line_sensor` 实施规格

建议接口：

```c
#define LINE_SENSOR_COUNT        8U
#define LINE_SENSOR_STRENGTH_MAX 1000U
```

配置：

```c
typedef struct {
    uint8_t channelMap[LINE_SENSOR_COUNT];

    uint16_t backgroundValue[LINE_SENSOR_COUNT];
    uint16_t lineValue[LINE_SENSOR_COUNT];

    int16_t positionWeight[LINE_SENSOR_COUNT];

    uint16_t minimumCalibrationRange;
    uint32_t minimumTotalStrength;
} LineSensor_Config;
```

结果：

```c
typedef struct {
    uint16_t raw[LINE_SENSOR_COUNT];
    uint16_t strength[LINE_SENSOR_COUNT];

    int32_t position;
    uint32_t totalStrength;

    bool valid;
} LineSensor_Result;
```

对象：

```c
typedef struct {
    LineSensor_Config config;
    LineSensor_Result result;

    int32_t lastValidPosition;
    bool initialized;
} LineSensor;
```

公开接口：

```c
bool LineSensor_Init(
    LineSensor *sensor,
    const LineSensor_Config *config);

bool LineSensor_IsConfigValid(
    const LineSensor_Config *config);

bool LineSensor_ProcessRaw(
    LineSensor *sensor,
    const uint16_t adcRaw[LINE_SENSOR_COUNT]);

const LineSensor_Result *LineSensor_GetResult(
    const LineSensor *sensor);
```

通道映射语义：

```c
physical[i] = adcRaw[channelMap[i]];
```

其中：

```text
physical[0] 必须是最左传感器
physical[7] 必须是最右传感器
```

建议位置权重：

```text
-3500
-2500
-1500
-500
500
1500
2500
3500
```

归一化：

```c
int32_t numerator =
    (int32_t)raw - (int32_t)background;

int32_t denominator =
    (int32_t)line - (int32_t)background;

int32_t strength =
    numerator * 1000 / denominator;
```

最后限制到：

```text
0～1000
```

因此同时支持：

```text
黑线 ADC 高于背景
黑线 ADC 低于背景
```

质心：

```text
position =
    Σ(strength[i] × weight[i])
    /
    Σ(strength[i])
```

中间量：

```c
int64_t weightedSum;
uint32_t totalStrength;
```

无效帧：

```text
result.valid = false
result.position = lastValidPosition
```

不能把位置清零，否则调试时会误以为线回到了中心。

---

# 17. `servo` 实施规格

配置必须包含真实硬件资源：

```c
typedef struct {
    GPTIMER_Regs *pwmTimer;
    DL_TIMER_CC_INDEX pwmChannel;
    uint32_t timerClockHz;

    uint16_t minimumPulseUs;
    uint16_t centerPulseUs;
    uint16_t maximumPulseUs;

    bool inverted;
} Servo_Config;
```

公开接口：

```c
bool Servo_Init(
    Servo *servo,
    const Servo_Config *config);

bool Servo_Enable(Servo *servo);

void Servo_Disable(Servo *servo);

bool Servo_SetNormalized(
    Servo *servo,
    int16_t command);

bool Servo_SetPulseUs(
    Servo *servo,
    uint16_t pulseUs);

bool Servo_Center(Servo *servo);

int16_t Servo_GetCommand(
    const Servo *servo);

uint16_t Servo_GetPulseUs(
    const Servo *servo);

bool Servo_IsEnabled(
    const Servo *servo);
```

归一化命令：

```text
-1000：一个方向安全极限
0：中位
1000：另一个方向安全极限
```

左右两段分别插值，因为中位不一定是两个极限的算术平均。

`Servo_Init()`：

```text
验证 min < center < max
保存配置
预装中位脉宽
不擅自启动定时器
enabled = false
```

`Servo_Enable()`：

```text
重新写入当前安全脉宽
启动舵机 PWM 定时器
enabled = true
```

`Servo_Disable()`：

```text
停止 PWM
舵机失去保持力
保留当前目标命令
```

---

# 18. `pid` 实施规格

配置：

```c
typedef struct {
    float kp;
    float ki;
    float kd;

    float integralMinimum;
    float integralMaximum;

    float outputMinimum;
    float outputMaximum;

    float derivativeFilterCoefficient;
} PID_Config;
```

公开接口：

```c
bool PID_Init(
    PID *pid,
    const PID_Config *config);

void PID_Reset(PID *pid);

float PID_UpdateError(
    PID *pid,
    float error,
    float dtSeconds);

float PID_Update(
    PID *pid,
    float target,
    float measurement,
    float dtSeconds);

float PID_GetLastOutput(
    const PID *pid);
```

行为：

```text
dt <= 0 时不除零
第一次更新微分项为 0
积分限幅
输出限幅
Reset 清除积分、历史误差、滤波状态和上次输出
```

今天不实现：

```text
自动整定
增量式 PID
复杂抗饱和模式
多套在线参数管理
```

---

# 19. `line_control` 实施规格

状态：

```c
typedef enum {
    LINE_CONTROL_TRACKING = 0,
    LINE_CONTROL_HOLDING,
    LINE_CONTROL_LINE_LOST,
    LINE_CONTROL_INVALID_ARGUMENT
} LineControl_Status;
```

配置：

```c
typedef struct {
    PID_Config steeringPid;

    float positionFullScale;
    int16_t maximumSteeringCommand;

    bool steeringInverted;

    uint8_t maximumInvalidFrames;
} LineControl_Config;
```

今天：

```text
maximumInvalidFrames = 3
```

精确定义：

```text
第 1 帧无效：
    HOLDING

第 2 帧无效：
    HOLDING

第 3 帧无效：
    LINE_LOST
    转向命令清零
```

有效帧重新出现：

```text
无效计数清零
恢复 TRACKING
```

线位置归一化：

```c
float normalizedPosition =
    (float)line->position
    / positionFullScale;
```

误差定义：

```c
error = -normalizedPosition;
```

PID 输出建议：

```text
-1.0～1.0
```

最终只转换一次：

```c
steeringCommand =
    pidOutput * maximumSteeringCommand;
```

方向反相只在 `steeringInverted` 中处理。

`HOLDING` 状态保持上一次转向命令。

---

# 20. `chassis` 实施规格

配置包含：

```text
左电机
右电机
公共 Standby 对象
转向舵机
最大驱动输出
最大转向命令
左右开环修正
```

左右开环修正名称应明确为：

```text
leftOpenLoopScalePermille
rightOpenLoopScalePermille
```

只服务于当前没有编码器闭环的阶段。

建议公开接口：

```c
bool Chassis_Init(
    Chassis *chassis,
    const Chassis_Config *config);

bool Chassis_Enable(
    Chassis *chassis,
    uint32_t nowMs);

void Chassis_Disable(
    Chassis *chassis,
    uint32_t nowMs);

void Chassis_SetOpenLoop(
    Chassis *chassis,
    int16_t driveOutput,
    int16_t steeringCommand,
    uint32_t nowMs);

void Chassis_Brake(
    Chassis *chassis,
    uint32_t nowMs);

void Chassis_Coast(
    Chassis *chassis,
    uint32_t nowMs);

void Chassis_CenterSteering(
    Chassis *chassis);

void Chassis_Process(
    Chassis *chassis,
    uint32_t nowMs);
```

使用现有 `motor.h` 中的真实类型名和函数名，不创造替代类型。

`Chassis_Enable()` 安全顺序：

```text
左右电机先设置短路制动
舵机设置中位
写入安全 PWM
启动舵机 PWM
启动电机 PWM
最后拉高 STBY
```

`Chassis_Process()` 每次主循环执行：

```text
Motor_Process(left)
Motor_Process(right)
```

当前开环阶段：

```text
两个后轮使用相同基础输出
再乘各自开环修正比例
```

今天不实现阿克曼内外侧后轮理论速度差。

---

# 21. `application` 实施规格

状态：

```c
typedef enum {
    APPLICATION_IDLE = 0,
    APPLICATION_RUNNING,
    APPLICATION_LINE_LOST,
    APPLICATION_ADC_ERROR,
    APPLICATION_FAULT
} Application_State;
```

公开接口：

```c
void Application_Init(void);

void Application_Process(void);

Application_State Application_GetState(void);
```

应用层保存：

```text
当前状态
已处理控制序号
漏周期计数
ADC 错误计数
上次遥测时间
遥测请求
当前开环驱动输出
```

HC-05 最小命令：

```text
s：
    从 IDLE 开始低速巡迹
    重置 PID 和丢线计数
    进入 RUNNING

x：
    立即短路制动
    舵机回中
    重置控制器
    回到 IDLE

t：
    输出一次八路原始值、归一化值和线位置
```

第一版 `t` 建议只输出一次，避免持续串口打印占用控制时间。

控制步骤：

```text
读取 ADC 八通道
ADC 错误则：
    adcErrorCount++
    Brake
    Servo_Center
    ADC_ERROR

LineSensor_ProcessRaw
数据处理失败则：
    Brake
    Servo_Center
    FAULT

非 RUNNING：
    只更新传感器
    不驱动车辆

RUNNING：
    LineControl_Update

TRACKING：
    使用新转向命令

HOLDING：
    保持上次转向
    继续低速行驶

LINE_LOST：
    Brake
    Servo_Center
    LINE_LOST
```

ADC 超时不设容忍窗口，立即进入 `ADC_ERROR`。

---

# 22. 上电安全要求

上电不得自动行驶。

初始化完成后必须保持：

```text
APPLICATION_IDLE
左右电机短路制动
舵机安全中位
```

推荐初始化顺序：

```text
1. SYSCFG_DL_init()
2. 初始化软件对象
3. 将电机方向引脚设为制动组合
4. 将电机 PWM 写为 0
5. 将舵机脉宽预装为安全中位
6. 启动必要 PWM 定时器
7. 输出确认安全后拉高 TB6612 STBY
8. 保持 IDLE，等待 HC-05 的 s 命令
```

任何故障：

```text
左右后轮短路制动
舵机回中
保持 PWM 以维持中位
```

---

# 23. `main.c` 目标

根目录 `main.c` 尽量薄：

```c
#include "ti_msp_dl_config.h"
#include "application.h"

int main(void)
{
    SYSCFG_DL_init();
    Application_Init();

    while (1) {
        Application_Process();
        __WFI();
    }
}
```

`SysTick_Handler()` 只调用：

```c
SystemTime_On1msTick();
```

不能在 ISR 中：

```text
运行 PID
读取 ADC
发送串口
调用巡迹
调用应用状态机
做浮点控制
```

---

# 24. Makefile 要求

根 Makefile：

```text
只生成一个最终 ELF
编译根 main.c
编译 00_generated
编译 myownlib 中正式模块
不编译 Examples/
不编译 tests/host/
```

至少支持：

```text
make
make clean
make sysconfig
```

可选：

```text
make flash
make size
make host-test
```

SysConfig 输出固定进入：

```text
00_generated/
```

---

# 25. 今天必须实测的内容

## 25.1 八路物理顺序

不能假设 ADC0_A0～A7 就是从左到右。

测试：

```text
只遮挡最左侧探头
观察哪一路 ADC 变化
逐路确认
填写 channelMap[]
```

---

## 25.2 每路黑白标定

分别记录：

```text
背景值
轨迹线值
```

每一路单独配置：

```c
backgroundValue[8]
lineValue[8]
```

不要使用统一阈值。

---

## 25.3 舵机安全范围

车轮悬空，电机关闭。

步骤：

```text
寻找机械中位
逐步增加脉宽到安全左极限
回到中位
逐步减小到安全右极限
记录三项数据
```

不能直接套用未经验证的：

```text
1000～2000 μs
```

连杆可能提前顶死。

---

## 25.4 转向符号

验证：

```text
线在左边
→ 舵机应使车向左转

线在右边
→ 舵机应使车向右转
```

若相反，只修改配置中的方向反相，不在应用层添加临时负号。

---

## 25.5 后轮方向

抬起后轮验证：

```text
正驱动命令
→ 左右后轮都使车辆向前
```

通过 `Motor_Config.inverted` 修正。

---

# 26. 今天的调试顺序

```text
1. 编译统一工程
2. 验证上电保持安全静止
3. HC-05 输出八路 ADC
4. 确认 channelMap
5. 测量每路背景值和轨迹线值
6. 验证归一化范围接近 0～1000
7. 验证线从左到右时 position 单调负到正
8. 电机保持制动，只测试舵机跟随
9. 抬起后轮，确认电机方向
10. 低输出上直线
11. 调小范围 P
12. 出现振荡时降低 P 或加入少量 D
13. 验证三帧丢线后立即制动并回中
14. 验证 ADC 超时立即制动
```

---

# 27. 今天的验收标准

```text
八路 ADC 数值稳定
物理顺序正确
每路黑白标定独立有效
归一化方向一致
线位置从左到右单调变化
舵机方向正确
后轮前进方向一致
上电不自动行驶
收到 x 命令立即制动
ADC 故障立即制动
连续三帧丢线后制动并回中
小车能低速通过直线
小车能通过一个缓弯
没有明显高频左右摆动
```

今天不以“完整跑完所有赛道”为验收目标。

---

# 28. 主机测试要求

## `line_sensor`

至少覆盖：

```text
channelMap 重复，初始化失败
黑线值高于背景，归一化正确
黑线值低于背景，归一化正确
中央两路相同，位置约为 0
最左单路有效，位置接近最左权重
最右单路有效，位置接近最右权重
总强度不足，valid=false
无效帧保留上次有效位置
超出标定范围时饱和到 0 或 1000
```

## `pid`

至少覆盖：

```text
纯 P 输出
输出上限
输出下限
积分上限
第一次调用无微分突变
dt=0 不除零
Reset 清除历史状态
```

## `line_control`

至少覆盖：

```text
中央输出约 0
线偏左时符号正确
线偏右时符号正确
steeringInverted 反向正确
第 1 次无效返回 HOLDING
第 2 次无效返回 HOLDING
第 3 次无效返回 LINE_LOST
LINE_LOST 时命令归零
重新有效后恢复 TRACKING
输出不超过最大转向命令
```

## `system_time`

至少覆盖：

```text
9 次 tick，控制序号不变
第 10 次 tick，序号加 1
20 次 tick，序号为 2
ElapsedMs 在 uint32_t 回绕处正确
```

---

# 29. 当前未知但不阻碍今日工作的参数

```text
电机额定电压
电机减速比
电机空载转速
编码器具体型号
编码器输出电平
最终每轮 x4 计数
舵机中位脉宽
舵机左右安全极限
舵机脉宽与真实转角关系
更精确的轮径
更精确的轮距
更精确的轴距
MPU6050 安装轴向
摆杆机构最终结构
```

这些必须通过商品资料或实测补充，禁止猜测。

---

# 30. 已否决或暂缓的设计

## 不新建重复 `adc_sequence`

直接扩展现有：

```text
01_platform/adc
```

---

## 不让 `line_sensor` 读取 ADC

应用层组合：

```c
ADC_ReadSequence8(...);
LineSensor_ProcessRaw(...);
```

---

## 不使用千分比 PWM 驱动舵机精细脉宽

50 Hz 下千分比每格约 20 μs，精度偏粗。

必须增加：

```c
PwmOutput_SetHighTicks();
PwmOutput_SetPulseUs();
```

---

## 不用简单 `bool controlPending`

使用控制序号，以便统计漏周期。

---

## 不自动上电行驶

默认 `IDLE`，等待显式启动命令。

---

## 不把编码器、轮速环和里程计塞进今天的巡迹链

今天只做开环低速巡迹。

---

## 不把阿克曼底盘改成差速转向模型

后轮独立驱动不代表通过左右轮差速完成主要转向。

主要转向执行器是前轮舵机。

---

## 不过早实现万能驱动框架

当前先匹配真实 TB6612、真实定时器和真实接口。

遇到第二种硬件拓扑后再抽象共同部分。

---

## 不增加模糊 `Motor_Stop()`

明确使用：

```text
Motor_Brake()
Motor_Coast()
Motor_StandbyDisable()
```

---

# 31. GPT 与 Codex 的协作分工

## GPT 负责

```text
架构边界
接口设计
状态语义
控制逻辑
边界条件
整数溢出检查
单位和符号约定
测试用例
实现顺序
代码 diff 审查
故障分析
调参策略
后续模块规划
```

## Codex 负责

```text
读取本地真实代码
匹配现有类型名
匹配现有函数名
调用 SDK 2.11.00.07 的真实 DriverLib API
修改文件
运行 SysConfig
运行编译
运行测试
修复编译错误
输出 diff 和测试结果
```

推荐 Codex 每次只实施一个或一组紧密相关模块：

```text
adc
→ output
→ system_time
→ line_sensor + host test
→ servo
→ pid + host test
→ line_control + host test
→ chassis
→ application
→ main + Makefile
```

Codex 不需要重复思考整个架构，也不要重构无关代码。

---

# 32. 交给 Codex 的推荐提示词模板

```text
请只实现指定模块，不修改其他层。

开始前：
1. 读取本地现有头文件、实现和 all.syscfg；
2. 使用现有真实类型和函数名；
3. 保留已有公开接口；
4. 不创建重复模块；
5. 不重构无关代码。

严格遵循 GPT_LOG.md 中该模块的接口、状态语义和错误处理。

要求：
1. 使用 MSPM0 SDK 2.11.00.07 的真实 DriverLib API；
2. 不无限等待；
3. 不在 ISR 中运行控制算法或串口输出；
4. 编译并运行适用测试；
5. 报告修改文件；
6. 报告关键实现；
7. 报告编译和测试结果；
8. 提供简洁 diff 摘要；
9. 遇到现有接口与日志冲突时，先报告，不擅自重设计。
```

---

# 33. 新对话接手指令

新的 GPT 或其他模型接手时，优先执行：

```text
1. 阅读 GPT_LOG.md
2. 阅读最新 Codex 日志
3. 查看 git status
4. 查看最近提交
5. 查看当前 all.syscfg
6. 查看当前 motor、adc、output 接口
7. 查看 Codex 最近生成的 diff
8. 根据实际代码更新本日志
```

不得重新询问：

```text
主控型号
SDK 版本
底盘是否差速
电机驱动型号
PWM 引脚
舵机引脚
八路 ADC 引脚
控制周期
今天是否接编码器
line_sensor 是否直接读 ADC
是否使用简单 controlPending
```

这些已经确定。

---

# 34. 下一阶段预览

八路开环巡迹稳定后，优先顺序预计为：

```text
编码器 x4 解码
→ PPS
→ 每轮计数标定
→ wheel_control
→ 左右轮速度闭环
→ chassis 速度和曲率接口
→ 舵机实际转角标定
→ 阿克曼里程计
→ 急弯与丢线恢复
→ 赛道特殊标志识别
→ 摆杆设备驱动
→ 摆杆控制
→ H 题完整状态机
```

摆杆控制的架构尚未锁定，必须根据正式题面、机构形式、传感器和执行器确定。

---

# 35. 核心原则摘要

```text
先实现真实需要，不造万能框架
先安全静止，再允许运动
先让数据可观察，再调控制
配置处理方向和差异，上层不散布负号
平台层不理解设备语义
设备层不理解赛题流程
算法层不依赖硬件
应用层负责组合和故障策略
ISR 只做最少工作
控制漏周期只统计，不补跑旧数据
物理参数未知时拒绝猜测
阿克曼底盘按阿克曼模型设计
```

---

# 36. 当前最关键的控制链

```text
ADC_ReadSequence8()
    ↓
LineSensor_ProcessRaw()
    ↓
LineControl_Update()
    ↓
Servo_SetNormalized()
    ↓
Chassis_SetOpenLoop()
    ↓
Motor_SetOutput()
```

控制失败时：

```text
Chassis_Brake()
Servo_Center()
进入对应故障状态
```

这条链是当前第一优先级。

````

这份日志可以和 Codex 自己维护的日志并列保存，例如：

```text
GPT_LOG.md
CODEX_LOG.md
````

更稳妥的做法是让 Codex 每完成一个模块，就在 `GPT_LOG.md` 最上方追加一小段“实际实现状态”，避免设计文档和真实代码逐渐分离。
