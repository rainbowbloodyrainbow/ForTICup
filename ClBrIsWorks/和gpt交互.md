我：现在有一个半重大危机，我们老师说规则其实要求的只是红外传感器，不允许灰度传感器，本来都是ADC输入，可以不用改的，但我们红外模块只有五路，没有八路，所以需要改改，这是半个；剩下的一个是五路巡迹对于阿克曼结构不够精细了，好在我们是差速、阿克曼二合一小车，把舵机前轮拆掉换成万向轮就好了，只是代码又要改

gpt:
这不算大危机，反而是**传感器侧变简单、底盘控制侧换模型**。

最重要的结论：

> **五路红外可以继续使用模拟量加权质心；换成差速底盘后，不再输出舵机转向，而是输出左右轮差速。**

你们前面做好的 `motor、PID、system_time、UART、line_sensor 的基本数学思路` 都没有白做。

---

# 一、ADC 反而可能不需要“双序列合并”了

之前要八路，扩展板上很难在同一个 ADC 实例中找到八个合适通道，所以才考虑：

```text
ADC0 若干路
+
ADC1 若干路
→ 合并成 raw[8]
```

现在只有五路，优先在 SysConfig 中找**同一个 ADC 实例的五个空闲模拟通道**，直接配置成五通道序列：

```text
ADC1 五通道序列
→ raw[5]
```

MSPM0G3507 有两个独立的 12 位 ADC，资源上完全够；最终只需要看扩展板引出的哪些模拟引脚最方便，并让 SysConfig 校验通过。([Texas Instruments][1])

因此建议：

```c
ADC_Status ADC_ReadSequence5(
    ADC12_Regs *adc,
    uint16_t result[5]);
```

原来的 `ADC_ReadSequence8()` 可以留着，不必删除。

只有在找不到五个同属一个 ADC 的方便接口时，才退回两个序列合并。

---

# 二、五路巡迹是否够用

对阿克曼确实比较吃力，因为：

* 不能原地转向；
* 修正半径有限；
* 线到了最外侧传感器以后，仍需要预判；
* 五路横向分辨率比八路低。

但对差速车，五路模拟红外通常足以先完成稳定巡迹，因为它可以：

* 一侧减速；
* 一侧加速；
* 必要时单轮停止；
* 后续甚至可以反向一侧，完成小半径旋转。

尤其你们不是五路数字量，而是仍然读 ADC 模拟值，就可以继续通过加权质心获得比“5 个开关状态”更细的线位置。

建议权重改为：

```c
{-2000, -1000, 0, 1000, 2000}
```

或者根据传感器真实间距填写：

```c
{-2d, -d, 0, d, 2d}
```

绝对数值不重要，左右对称和间距比例正确更重要。

---

# 三、哪些代码不需要改

以下模块主体可以直接保留：

```text
01_platform/system_time
01_platform/output
02_device/motor
03_algorithm/pid
UART / HC-05
MPU6050
```

`line_sensor` 的归一化公式、总强度和加权质心算法也不变，只需把数量从 8 改成 5。

`servo` 可以保留在仓库中，但不再进入当前底盘运行链。不要删除，后续摆杆或者其他机构仍可能用到 PWM 舵机接口。

---

# 四、`line_sensor` 要怎么改

现在很可能有：

```c
#define LINE_SENSOR_COUNT 8U
```

改成：

```c
#define LINE_SENSOR_COUNT 5U
```

相应配置改成五项：

```c
typedef struct {
    uint8_t channelMap[5];

    uint16_t backgroundValue[5];
    uint16_t lineValue[5];

    int16_t positionWeight[5];

    uint16_t minimumCalibrationRange;
    uint32_t minimumTotalStrength;
} LineSensor_Config;
```

应用配置：

```c
#define APPLICATION_LINE_CHANNEL_MAP \
    {0U, 1U, 2U, 3U, 4U}

#define APPLICATION_LINE_POSITION_WEIGHTS \
    {-2000, -1000, 0, 1000, 2000}
```

黑白值仍然每路单独标定：

```c
#define APPLICATION_LINE_BACKGROUND_VALUES \
    { ..., ..., ..., ..., ... }

#define APPLICATION_LINE_VALUES \
    { ..., ..., ..., ..., ... }
```

## `minimumTotalStrength` 要重新标定

不能沿用八路时的阈值，因为总通道数少了。

最可靠的方法是测：

```text
最强的“全背景”总强度
最弱的“确认看见线”总强度
```

阈值取两者中间：

```text
minimumTotalStrength
    = (maxBackgroundTotal + minValidLineTotal) / 2
```

不要凭感觉直接除以 `8/5`。

---

# 五、`line_control` 算法几乎不用改

原来它输出：

```text
steeringCommand
```

现在改成：

```text
turnCommand
```

或者：

```text
correctionCommand
```

计算仍然可以是：

```c
float normalizedPosition =
    (float)line->position /
    control->config.positionFullScale;

float error = -normalizedPosition;

float turn =
    PID_UpdateError(
        &control->pid,
        error,
        dtSeconds);
```

现有方向定义是合理的：

```text
线在左边：position < 0
线在右边：position > 0

左转：turn > 0
右转：turn < 0
```

因为：

```c
error = -position;
```

所以线在左侧时，误差为正，控制器要求左转。

建议重命名：

```c
int16_t LineControl_GetTurnCommand(
    const LineControl *control);
```

但比赛时间紧，内部变量即使暂时还叫 `steeringCommand`，功能也能运行；只是以后容易混淆。

---

# 六、`chassis` 要从阿克曼改成差速

这是主要的软件修改。

原来：

```text
基础驱动输出
+
舵机转向
```

现在：

```text
基础驱动输出
+
差速转向量
→ 左右电机输出
```

## 推荐接口

```c
void Chassis_SetDriveTurn(
    Chassis *chassis,
    int16_t driveOutput,
    int16_t turnOutput,
    uint32_t nowMs);
```

再保留一个底层调试接口：

```c
void Chassis_SetWheelOutputs(
    Chassis *chassis,
    int16_t leftOutput,
    int16_t rightOutput,
    uint32_t nowMs);
```

## 差速混合公式

按照既定约定：

```text
turn > 0：左转
turn < 0：右转
```

应使用：

```c
leftOutput  = driveOutput - turnOutput;
rightOutput = driveOutput + turnOutput;
```

验证一下：

### 要左转

```text
turn > 0

左轮输出减小
右轮输出增大
→ 车辆左转
```

### 要右转

```text
turn < 0

左轮输出增大
右轮输出减小
→ 车辆右转
```

这与现有：

```c
error = -linePosition;
```

是匹配的。

---

# 七、第一版先禁止反转

理论上差速车可以让一侧反转，转弯会非常灵活；但第一版巡迹不建议立即允许。

先限制为：

```c
leftOutput  = Clamp(leftOutput,  0, maxOutput);
rightOutput = Clamp(rightOutput, 0, maxOutput);
```

这样最多出现：

```text
一侧前进
另一侧停止
```

不会因为 PID 突然输出过大，导致小车高速原地旋转。

基础巡迹稳定后，再考虑允许：

```text
-小幅反转 ～ 正向最大输出
```

用于特别急的弯道。

## 混合代码示例

```c
void Chassis_SetDriveTurn(
    Chassis *chassis,
    int16_t drive,
    int16_t turn,
    uint32_t nowMs)
{
    int32_t left  = (int32_t)drive - turn;
    int32_t right = (int32_t)drive + turn;

    /* 第一阶段只允许前进或单轮停止。 */
    left = Clamp(left, 0, chassis->config.maximumDriveOutput);
    right = Clamp(right, 0, chassis->config.maximumDriveOutput);

    left =
        left *
        chassis->config.leftOpenLoopScalePermille /
        1000;

    right =
        right *
        chassis->config.rightOpenLoopScalePermille /
        1000;

    Motor_SetOutput(
        chassis->config.leftMotor,
        (int16_t)left,
        nowMs);

    Motor_SetOutput(
        chassis->config.rightMotor,
        (int16_t)right,
        nowMs);
}
```

---

# 八、`Chassis_Config` 可以去掉舵机依赖

当前可能是：

```c
typedef struct {
    Motor *leftMotor;
    Motor *rightMotor;
    Servo *steeringServo;
    ...
} Chassis_Config;
```

改为：

```c
typedef struct {
    Motor *leftMotor;
    Motor *rightMotor;

    MotorStandby *standby; /* 使用真实类型名 */

    uint16_t maximumDriveOutput;
    uint16_t maximumTurnOutput;

    uint16_t leftOpenLoopScalePermille;
    uint16_t rightOpenLoopScalePermille;
} Chassis_Config;
```

删除或暂时停用：

```c
Chassis_CenterSteering();
```

故障状态只需要：

```c
Chassis_Brake(&gChassis, nowMs);
```

舵机不再参与安全状态。

不过为了少改代码，也可以暂时让：

```c
Servo *steeringServo;
```

允许为 `NULL`，所有舵机操作先判断空指针。长期看，既然正式底盘已改成差速，直接从 `chassis` 中移除会更清晰。

---

# 九、`application` 的控制步骤怎么改

原来：

```c
int16_t steering =
    LineControl_GetSteeringCommand(&gLineControl);

Chassis_SetOpenLoop(
    &gChassis,
    gDriveOutput,
    steering,
    nowMs);
```

现在：

```c
int16_t turn =
    LineControl_GetTurnCommand(&gLineControl);

Chassis_SetDriveTurn(
    &gChassis,
    gDriveOutput,
    turn,
    nowMs);
```

丢线时：

```c
Chassis_Brake(&gChassis, nowMs);
```

不再调用：

```c
Servo_Center();
```

ADC 读取改成：

```c
uint16_t raw[5];

ADC_Status status =
    ADC_ReadSequence5(
        LINE_ADC_INST,
        raw);
```

然后：

```c
LineSensor_ProcessRaw(
    &gLineSensor,
    raw);
```

---

# 十、机械改造时的注意点

把前部阿克曼机构拆掉、换万向轮后，建议确认：

* 两个驱动轮仍在同一轴线上；
* 万向轮转动和旋转都足够顺滑；
* 万向轮不要承受过大重量，否则转向迟钝；
* 光电阵列安装在驱动轮轴线前方；
* 阵列与地面保持平行；
* 五个探头横向覆盖宽度要大于轨迹线宽；
* 线在中央时，中间探头正对轨迹中心。

五路传感器精度不足时，首先优化：

1. 安装高度；
2. 阵列前探距离；
3. 车速；
4. 黑白标定；
5. P、D 参数；

不要第一时间认定必须增加传感器数量。

---

# 十一、差速底盘的调试顺序

## 1. 单独验证传感器

```text
黑线从最左移动到最右
position 应从负值单调变化到正值
```

## 2. 后轮悬空，先不跑自动巡迹

直接测试：

```text
drive=100, turn=0
→ 两轮同方向

drive=100, turn=30
→ 左轮较慢，右轮较快，代表左转

drive=100, turn=-30
→ 左轮较快，右轮较慢，代表右转
```

## 3. 验证控制方向

手动移动黑线：

```text
黑线偏左
→ 右轮变快、左轮变慢

黑线偏右
→ 左轮变快、右轮变慢
```

如果相反，只修改一个统一的：

```c
LineControl_Config.inverted
```

不要交换多个负号。

## 4. 落地低速测试

建议初始：

```text
driveOutput：刚刚足够稳定前进
maximumTurnOutput：driveOutput 的 40%～60%
Ki：0
Kd：0
Kp：从小开始
```

例如基础速度是 150，第一轮可把最大转向修正限制在约 60～90，而不是直接允许 1000。

---

# 十二、下一优先级变成了编码器闭环

阿克曼底盘中，左右后轮速度不一致主要造成轻微偏航，舵机仍是主要转向执行器。

差速底盘中：

> **左右轮速度差本身就是转向。**

所以左右电机固有差异、电池压降、摩擦差异会直接影响巡迹。

第一天仍然可以用：

```text
开环基础输出
+
左右 openLoopScale
+
巡迹差速修正
```

先跑起来。

但基础巡迹成功后，下一优先级应立即变成：

```text
编码器
→ PPS
→ 左右轮速度 PI
→ 差速底盘速度控制
```

届时 `line_control` 输出目标转向量，再转换为左右目标轮速，而不是直接转换成 PWM。

---

# 十三、这次改动涉及哪些文件

## 必须修改

```text
all.syscfg
01_platform/adc
02_device/line_sensor
04_control/line_control
05_robot/chassis
07_application/application
07_application/application_config
相关 host tests
```

## 基本不改

```text
02_device/motor
03_algorithm/pid
01_platform/system_time
UART / HC-05
MPU6050
```

## 暂时退出运行链，但保留源码

```text
02_device/servo
转向舵机配置
```

---

# 十四、现在最合理的动作顺序

1. 给当前阿克曼版本打 tag 或建分支：

```bash
git tag ackermann-line-stage
```

2. 将机械底盘改成两轮差速＋万向轮。

3. 先在 SysConfig 里配置五路同一 ADC 的红外模拟输入。

4. 把 `LINE_SENSOR_COUNT` 改成 5，并更新测试。

5. 将 `chassis` 改为 `drive + turn` 差速混合。

6. 修改 Application 调用。

7. 后轮悬空验证左右混合方向。

8. 完成五路红外黑白标定。

9. 低速落地调 P。

10. 基础巡迹成功后马上开发编码器轮速闭环。

这次危机的实际结果反而是：

```text
八路双 ADC 采样
→ 简化成五路单 ADC

阿克曼舵机转向
→ 改成差速混合
```

**改动集中在 ADC 数量和 `chassis/application`，不需要推倒整个架构。**

[1]: https://www.ti.com/product/ja-jp/MSPM0G3507?utm_source=chatgpt.com "MSPM0G3507 のデータシート、製品情報、およびサポート | TI.com"
