我：我现在要先让2204无刷云台电机转起来，并实现闭环控制。电机本身的红黑黄三相线，simplefoc mini电机驱动板，AS5600编码器，地猛星，它们该如何接线，又该怎么分配地猛星的引脚呢（我准备单独拿一个地猛星去接收串口传来的偏差量信息，然后去控制电机）



gpt:
可以，建议你把这块地猛星专门做成一个**摆杆伺服控制器**：

* TIMA0 输出三相同步 PWM；
* I²C 读取 AS5600；
* UART1 接收上位控制器给出的目标量；
* UART0 保留给板载 USB 串口调试。

先按下面这套分配，不容易冲突，而且布线也比较集中。

## 一、推荐引脚分配

| 功能       | 地猛星引脚     | 外设功能     | 接到哪里                 |
| -------- | --------- | -------- | -------------------- |
| U 相 PWM  | PA21      | TIMA0_C0 | SimpleFOC Mini `IN1` |
| V 相 PWM  | PA22      | TIMA0_C1 | SimpleFOC Mini `IN2` |
| W 相 PWM  | PA23      | TIMA0_C3 | SimpleFOC Mini `IN3` |
| 驱动使能     | PA24      | GPIO 输出  | SimpleFOC Mini `EN`  |
| 驱动故障，可选  | PA25      | GPIO 输入  | SimpleFOC Mini `nFT` |
| 编码器 SDA  | PA0       | I2C0_SDA | AS5600 `SDA`         |
| 编码器 SCL  | PA1       | I2C0_SCL | AS5600 `SCL`         |
| 外部串口发送   | PA8       | UART1_TX | 对方 MCU 的 RX          |
| 外部串口接收   | PA9       | UART1_RX | 对方 MCU 的 TX          |
| USB 调试串口 | PA10/PA11 | UART0    | 板载 Type-C 转串口        |

PA21、PA22、PA23分别可以映射成 TIMA0_C0、TIMA0_C1、TIMA0_C3。通道编号不连续没有关系，关键是它们属于同一个 TIMA0，可以共用同一个计数周期并同步更新。PA8、PA9可以分别作为 UART1_TX 和 UART1_RX；PA0、PA1则可作为 I2C0_SDA 和 I2C0_SCL。([Texas Instruments][1])

地猛星板载 USB 转串口连接的是 PA10、PA11，因此把它们保留给电脑调试，另外用 PA8、PA9接收视觉或主控数据，调试会方便很多。([立创开发板技术文档中心][2])

---

# 二、2204电机与SimpleFOC Mini接线

## 电机三相线

你说电机有红、黑、黄三根线，它们**全部都是电机相线**：

| 电机线 | SimpleFOC Mini |
| --- | -------------- |
| 红线  | M1             |
| 黑线  | M2             |
| 黄线  | M3             |

初次接线时顺序可以任意。

特别注意：

> **黑线不是电源负极，也不是GND。**

如果最终电机旋转方向和编码器方向不一致，可以：

* 软件中反转编码器方向；
* 或交换任意两根电机相线。

SimpleFOC Mini官方要求电机三相接到 `M1/M2/M3`，控制侧则需要 `IN1/IN2/IN3/EN/GND` 五根线。([Arduino-FOC][3])

## SimpleFOC Mini v1.0 控制排针

你照片上的是 **v1.0**，按板上丝印从下往上一般是：

```text
GND
IN1
IN2
IN3
EN
```

所以接成：

```text
地猛星 PA21 ─── IN1
地猛星 PA22 ─── IN2
地猛星 PA23 ─── IN3
地猛星 PA24 ─── EN
地猛星 GND  ─── GND
```

一定按你板子上的丝印确认。SimpleFOC Mini v1.0和v1.1的 `IN1/IN2/IN3/EN` 排列顺序发生过变化，不能直接照搬v1.1接线图。([Arduino-FOC][3])

---

# 三、AS5600接线

你这块圆形编码器板上有两组接口。

一组是：

```text
VCC  OUT  GND
```

另一组是：

```text
DIR  SCL  SDA  PGO
```

第一版建议使用I²C：

```text
AS5600 VCC ─── 地猛星 3V3
AS5600 GND ─── 地猛星 GND
AS5600 SDA ─── 地猛星 PA0
AS5600 SCL ─── 地猛星 PA1
```

其余引脚：

* `OUT`：暂时不接；
* `DIR`：暂时不接；
* `PGO`：不接。

AS5600的7位I²C地址是固定的 `0x36`，支持最高1MHz的I²C时钟。第一版先用400kHz，电机运行稳定后再考虑提高到1MHz。([Seeed Studio][4])

## 上拉电阻

SDA和SCL必须上拉到3.3V。

如果地猛星的PA0、PA1上拉选择开关已经设置为3.3V，可以直接使用。否则增加：

```text
SDA ── 4.7kΩ ── 3V3
SCL ── 4.7kΩ ── 3V3
```

不要把I²C上拉到5V。

## 先确认磁铁状态

正式控制电机前，先读取AS5600状态寄存器，检查：

* 磁铁是否检测到；
* 磁场是否过强；
* 磁场是否过弱；
* 手转电机时原始角度是否连续变化。

如果原始值从4095跳到0，这是正常的跨零点；软件里需要做多圈角度展开。

---

# 四、电源接法

这是最容易烧板的部分。

## SimpleFOC Mini动力电源

```text
限流电源正极  ─── SimpleFOC Mini “+”
限流电源负极  ─── SimpleFOC Mini “-”
```

SimpleFOC Mini v1.0要求8～35V动力电源。首次测试建议使用：

```text
电压：8V
限流：0.3～0.5A
```

先不要直接上满电电池。SimpleFOC Mini主要面向高内阻云台电机；官方还特别提醒，低内阻电机必须限制输出电压。([Arduino-FOC][3])

## 地猛星供电

地猛星单独通过：

* Type-C；
* 或稳定的5V输入

供电。

不要把SimpleFOC Mini的动力电源直接接到地猛星5V或3.3V。

## SimpleFOC Mini的3V3不要给地猛星供电

Mini板上的 `3V3` 是DRV8313内部稳压器的输出，官方给出的最大供电能力只有约10mA，不足以给地猛星供电，也不建议与地猛星自己的3.3V直接并联。([Arduino-FOC][3])

## 所有系统必须共地

最终需要连接在一起的是：

```text
动力电源负极
SimpleFOC Mini GND
地猛星 GND
AS5600 GND
发送偏差信息的另一块主控 GND
```

可以理解为：

```text
                         ┌── 地猛星 GND
动力电源负极 ── 公共地 ──┼── Mini GND
                         ├── AS5600 GND
                         └── 上位主控 GND
```

尽量让动力电流走：

```text
电源 → Mini → 电机
```

不要让大电流经过AS5600或地猛星的细地线。

---

# 五、可选故障信号

SimpleFOC Mini还提供：

* `nFT`：故障输出，低电平有效；
* `nRT`：复位，低电平有效；
* `nSP`：休眠，低电平有效。

第一版可以只连接：

```text
Mini nFT ─── 地猛星 PA25
```

把PA25设置为带上拉输入。

程序中检测：

```c
if (driver_fault_pin == 0) {
    driver_disable();
    motor_target = 0.0f;
}
```

`nRT`和`nSP`第一版可以不接。官方说明这三个引脚属于可选功能，其中 `nFT` 低电平表示驱动器进入故障状态。([Arduino-FOC][3])

---

# 六、UART接线

假设另一块地猛星、视觉板或主控向电机控制地猛星发送数据：

```text
发送主控 TX  ─── 电机控制板 PA9 / UART1_RX
发送主控 RX  ─── 电机控制板 PA8 / UART1_TX（可选）
发送主控 GND ─── 电机控制板 GND
```

只单向发送偏差时，可以只接：

```text
主控 TX → PA9
GND → GND
```

建议先用：

```text
115200 baud
8 data bits
1 stop bit
no parity
```

不要直接发送ASCII浮点数字作为最终协议。初期调试可以用字符串，正式控制建议使用固定帧，例如：

```c
typedef struct __attribute__((packed)) {
    uint8_t header1;       // 0xAA
    uint8_t header2;       // 0x55
    int16_t position_mm;   // 球位置或误差，单位0.1mm/1mm
    int16_t velocity_mm_s;
    uint16_t sequence;
    uint8_t checksum;
} BallPacket;
```

还要设置超时：

```c
if (uart_timeout_ms > 100) {
    target_beam_angle = 0.0f;
}
```

不能在串口断线后保持最后一个错误指令。

---

# 七、TIMA0该怎么配置

建议在SysConfig中配置一个高级定时器TIMA0。

## 基本配置

```text
计数模式：Up/Down，中心对齐
PWM频率：20kHz
通道：CC0、CC1、CC3
死区：关闭
输出极性：高有效
同步更新：开启
```

对应：

```text
TIMA0_C0 → PA21 → IN1
TIMA0_C1 → PA22 → IN2
TIMA0_C3 → PA23 → IN3
```

选择中心对齐的好处是三相PWM边沿更加对称。更重要的是：

> 三个比较值必须在同一个计数周期边界同时更新，不能依次写完就立即生效。

建议使用影子寄存器，在计数器到零或周期边界时统一装载新的三相占空比。

## EN的上电状态

PA24上电后应保持低电平：

```c
driver_enable(false);
```

顺序建议是：

1. 初始化GPIO；
2. EN保持低；
3. 初始化PWM；
4. 初始化I²C；
   5.确认AS5600正常；
5. 将三相PWM置为安全值；
6. 最后EN拉高。

---

# 八、第一阶段不要直接做位置闭环

按以下顺序逐级测试。

## 第一步：只测试AS5600

不接动力电源，手转电机：

```text
读取地址0x36
读取RAW_ANGLE
确认0～4095连续变化
确认转一圈后回到原值
```

同时检查磁铁状态。

## 第二步：只使能驱动，但不给力

接8V限流电源：

```text
EN = 0
PWM全部初始化
确认驱动不发热
确认nFT保持高
```

然后把软件输出电压限制设得非常低，例如等效：

```text
0.3～0.5V
```

不要一开始就把PWM幅值开到8V。

## 第三步：电角度对齐

需要确认电机的：

* 极对数；
* 相序；
* 编码器方向；
* 电角度零偏。

极对数不要因为它叫“2204”就直接假设为7或11。应当数转子磁极数量：

[
\text{极对数}=\frac{\text{磁极数量}}{2}
]

也可以通过低压电角度扫描自动辨识。

电角度关系为：

[
\theta_e=p\theta_m+\theta_{\text{offset}}
]

其中：

* (\theta_e)：电角度；
* (p)：极对数；
* (\theta_m)：AS5600机械角；
* (\theta_{\text{offset}})：对齐偏置。

## 第四步：先做电压/力矩控制

先让电机产生很小的正反保持力矩，不做位置环：

```text
正小力矩
零力矩
负小力矩
```

确认：

* 输出方向正确；
* 没有剧烈抖动；
* nFT不触发；
* 电机和驱动板不过热。

## 第五步：角度位置闭环

再做：

```text
当前角度
→ +5°
→ 0°
→ -5°
→ 0°
```

第一版位置控制可采用：

```c
torque_cmd =
    kp * angle_error
    - kd * motor_velocity;
```

把输出严格限制在很小范围内。

---

# 九、建议的控制周期

你的专用电机控制地猛星可以这样分层：

```text
PWM/FOC更新：10～20kHz
AS5600角度更新：1～5kHz，视I2C实际能力
角度/速度控制：1kHz
UART解析：中断或DMA
上位目标更新：30～100Hz
故障检测：1kHz
```

大致结构：

```c
while (1) {
    uart_process();
    fault_process();

    // 其他低优先级任务
}
```

高速部分放定时器中断：

```c
void TIMA0_IRQHandler(void)
{
    float mechanical_angle = encoder_get_angle();
    float electrical_angle =
        pole_pairs * mechanical_angle + electrical_offset;

    foc_update(electrical_angle);
}
```

不过，如果AS5600使用I²C，不建议在20kHz中断里每次都阻塞读取。更合理的是：

```text
I²C定期更新机械角
FOC高速循环使用最近一次角度＋速度外推
```

---

# 十、你现在实际需要接的最小系统

```text
                     ┌────────────────────┐
       PA21 ─────────│ IN1                │
       PA22 ─────────│ IN2  SimpleFOC Mini│── M1 ── 电机红线
       PA23 ─────────│ IN3                │── M2 ── 电机黑线
       PA24 ─────────│ EN                 │── M3 ── 电机黄线
地猛星 GND ──────────│ GND                │
                     │ +              -   │
                     └─┬──────────────┬───┘
                       │              │
                      8V+            电源负极
                                      │
                                      └──── 公共GND


地猛星 3V3 ───────── AS5600 VCC
地猛星 GND ───────── AS5600 GND
地猛星 PA0 ───────── AS5600 SDA
地猛星 PA1 ───────── AS5600 SCL


上位主控 TX ──────── 地猛星 PA9 / UART1_RX
上位主控 RX ──────── 地猛星 PA8 / UART1_TX
上位主控 GND ─────── 地猛星 GND
```

最终推荐的引脚宏可以先定成：

```c
#define FOC_PWM_U_PIN      PA21   // TIMA0_C0
#define FOC_PWM_V_PIN      PA22   // TIMA0_C1
#define FOC_PWM_W_PIN      PA23   // TIMA0_C3
#define FOC_ENABLE_PIN     PA24
#define FOC_FAULT_PIN      PA25

#define AS5600_SDA_PIN     PA0
#define AS5600_SCL_PIN     PA1
#define AS5600_I2C_ADDR    0x36

#define COMMAND_UART_TX    PA8
#define COMMAND_UART_RX    PA9
```

这套分配还保留了PA10、PA11板载USB串口，首次调试时可以一边从PA8/PA9接收指令，一边在电脑上观察：

```text
AS5600原始角度
机械角度
电角度
目标角度
实际角度
PWM_U/V/W
nFT状态
UART超时状态
```

先完成“手转电机，AS5600角度正确读取”，再接动力电源进行对齐；不要一次性把编码器、FOC、位置环和串口控制全部打开。

[1]: https://www.ti.com/lit/ds/symlink/mspm0g3507.pdf "MSPM0G350x Mixed-Signal Microcontrollers With CAN-FD Interface datasheet (Rev. C)"
[2]: https://wiki.lckfb.com/zh-hans/dmx/beginner/uart.html?utm_source=chatgpt.com "7. 串口通信"
[3]: https://docs.simplefoc.com/mini_v1_connect_hardware "Mini v1 | Arduino-FOC"
[4]: https://files.seeedstudio.com/wiki/Grove-12-bit-Magnetic-Rotary-Position-Sensor-AS5600/res/Magnetic%20Rotary%20Position%20Sensor%20AS5600%20Datasheet.pdf "AS5600 "



























我：再细说一下sysconfig里怎么配置。然后生成一个prompt，让codex给我写一份能闭环控制这个2204无刷云台电机的代码（具体表现为用Uart给它发一个角度或者什么，它就转到那里并保持住。如果可以Uart用PA9为rx，PA8为tx）

gpt：
可以。先明确一下目标：

> 你现在要做的是**带编码器的电压模式 FOC＋机械角度闭环**。
> AS5600负责转子角度反馈，外层位置/速度控制器输出 (U_q)，三相PWM驱动2204转到UART指定角度并保持。

SimpleFOC Mini没有相电流采样，因此这不是电流闭环FOC，但实现低速云台电机位置保持完全可行。SimpleFOC Mini需要三路PWM、一路EN和共地；你的v1.0板与v1.1的控制排针顺序不同，接线时必须以板上丝印为准。([Arduino-FOC][1])

---

# 一、最终外设分配

| 功能               | MSPM0G3507引脚 | SysConfig外设 |
| ---------------- | ------------ | ----------- |
| SimpleFOC IN1    | PA21         | TIMA0_C0    |
| SimpleFOC IN2    | PA22         | TIMA0_C1    |
| SimpleFOC IN3    | PA23         | TIMA0_C3    |
| SimpleFOC EN     | PA24         | GPIO输出      |
| SimpleFOC nFT，可选 | PA25         | GPIO输入      |
| AS5600 SDA       | PA0          | I2C0_SDA    |
| AS5600 SCL       | PA1          | I2C0_SCL    |
| 命令串口TX           | PA8          | UART1_TX    |
| 命令串口RX           | PA9          | UART1_RX    |
| 控制周期定时器          | 不占外部引脚       | TIMG0       |

PA21、PA22、PA23分别可以复用成TIMA0_C0、C1、C3；PA8、PA9分别支持UART1_TX、UART1_RX；PA0、PA1支持I2C0_SDA、SCL。([Texas Instruments][2])

---

# 二、SysConfig具体配置

不同版本MSPM0 SDK中的菜单名字可能略有区别，但配置内容一致。MSPM0 SysConfig会自动生成DriverLib初始化代码和PinMux配置，因此不要手动修改`ti_msp_dl_config.c/.h`。([软件下载][3])

## 1. 时钟配置

打开工程里的：

```text
xxx.syscfg
```

进入：

```text
System Configuration / SYSCTL / Clock Tree
```

建议：

* MCLK：保持地猛星工程默认配置；
* 如果当前模板已经是80MHz，不需要修改；
* TIMA0时钟选择BUSCLK；
* TIMG0时钟选择BUSCLK；
* UART和I2C也使用默认BUSCLK派生时钟。

第一版不要为了FOC额外改PLL或晶振配置。PWM频率由SysConfig按实际定时器时钟计算。

---

## 2. 配置三相PWM

添加：

```text
Timer → Timer PWM
```

实例命名建议：

```text
FOC_PWM
```

### 基础参数

| 参数                   | 设置                       |
| -------------------- | ------------------------ |
| Timer Instance       | TIMA0                    |
| PWM Mode             | Center-Aligned / Up-Down |
| PWM Frequency        | 20kHz                    |
| Clock Source         | BUSCLK                   |
| Dead Band            | Disabled                 |
| Complementary Output | Disabled                 |
| Output Polarity      | Active High              |
| Interrupt            | 暂时关闭                     |

如果界面要求输入周期而不是频率：

```text
PWM周期 = 50us
```

如果暂时找不到Center-Aligned，第一阶段也可以使用20kHz Edge-Aligned；电机先转起来后再改成中心对齐。

### 添加三个PWM通道

在`PWM Channels`或`Capture Compare Channels`中添加：

#### 通道一

```text
Channel：CC0
Output Pin：PA21
Initial Duty：50%
```

#### 通道二

```text
Channel：CC1
Output Pin：PA22
Initial Duty：50%
```

#### 通道三

```text
Channel：CC3
Output Pin：PA23
Initial Duty：50%
```

为什么初始化成50%：

三相占空比全部为50%时，各相之间线电压接近零，适合作为使能驱动器前的安全中点。真正运行时，FOC算法会围绕50%上下调制。

### 同步更新

如果SysConfig里有以下选项：

```text
Capture Compare Update Method
Shadow Register
Update on Zero Event
Load on Counter Zero
```

选择：

```text
三个通道在计数器Zero事件同时更新
```

不能让三相比较值分别立即生效，否则更新瞬间可能产生畸变。

### PinMux检查

进入SysConfig右侧或底部的PinMux视图，确认：

```text
PA21 = TIMA0_C0
PA22 = TIMA0_C1
PA23 = TIMA0_C3
```

如果PA23显示被VREF占用：

* 检查是否误添加了VREF外设；
* 删除不需要的VREF配置；
* 重新将PA23分配给TIMA0_C3。

---

## 3. 配置SimpleFOC控制GPIO

添加：

```text
GPIO
```

实例或组名建议：

```text
FOC_GPIO
```

### EN引脚

添加输出：

```text
Pin：PA24
Name：FOC_EN
Direction：Output
Initial Output：Low
Drive Strength：Default
```

上电必须保持低电平。程序完成：

1. PWM初始化；
2. AS5600通信检查；
3. 电角度对齐准备；

之后才能拉高EN。

### nFT故障引脚，可选但建议接

添加输入：

```text
Pin：PA25
Name：FOC_NFAULT
Direction：Input
Internal Pull：Pull-Up
Interrupt：Falling Edge，可选
```

SimpleFOC Mini的`nFT`为低电平有效，拉低说明DRV8313进入故障状态。([Arduino-FOC][1])

第一版可以不用中断，控制循环中轮询：

```c
if (nFAULT == 0) {
    motor_disable();
}
```

---

## 4. 配置AS5600 I²C

添加：

```text
I2C
```

实例命名：

```text
AS5600_I2C
```

设置：

| 参数                  | 值          |
| ------------------- | ---------- |
| Peripheral Instance | I2C0       |
| Mode                | Controller |
| Bus Speed           | 400kHz     |
| Address Mode        | 7-bit      |
| SDA                 | PA0        |
| SCL                 | PA1        |
| DMA                 | Disabled   |
| Interrupt           | 第一版关闭      |
| FIFO                | 默认或关闭      |
| Clock Stretch       | 默认允许       |

第一版使用阻塞式/轮询I²C最容易调试，但不要在中断函数中进行阻塞I²C读写。

### 外部上拉

即使SysConfig里能打开内部上拉，也建议使用外部：

```text
SDA → 4.7kΩ → 3.3V
SCL → 4.7kΩ → 3.3V
```

### AS5600软件参数

```c
#define AS5600_ADDRESS        0x36
#define AS5600_REG_STATUS     0x0B
#define AS5600_REG_RAW_ANGLE  0x0C
```

RAW ANGLE由：

```text
0x0C：高4位
0x0D：低8位
```

组成12位角度。

AS5600是12位绝对磁编码器，7位I²C地址固定为`0x36`，RAW ANGLE寄存器地址为`0x0C/0x0D`，STATUS地址为`0x0B`。

---

## 5. 配置UART1

添加：

```text
UART
```

实例命名：

```text
CMD_UART
```

### 基础设置

| 参数                    | 值        |
| --------------------- | -------- |
| UART Instance         | UART1    |
| Baud Rate             | 115200   |
| Data Bits             | 8        |
| Parity                | None     |
| Stop Bits             | 1        |
| Hardware Flow Control | Disabled |
| TX Pin                | PA8      |
| RX Pin                | PA9      |

也就是：

```text
PA8 = UART1_TX
PA9 = UART1_RX
```

注意交叉接线：

```text
上位主控TX → 地猛星PA9 / RX
上位主控RX ← 地猛星PA8 / TX
两块板GND必须相连
```

### RX中断

打开：

```text
Receive Interrupt
RX Interrupt
```

如果可以配置FIFO阈值：

```text
RX FIFO Threshold：1 byte
```

建议UART中断里只完成：

* 读取字节；
* 写入环形缓冲区；
* 清中断标志。

不要在UART ISR里：

* 解析浮点数；
* 调用`printf`；
* 执行FOC；
* 读AS5600。

---

## 6. 配置1kHz控制周期

添加：

```text
Timer → Timer Periodic
```

实例名：

```text
CONTROL_TICK
```

设置：

| 参数              | 值                       |
| --------------- | ----------------------- |
| Instance        | TIMG0                   |
| Mode            | Periodic                |
| Period          | 1ms                     |
| Frequency       | 1kHz                    |
| Clock Source    | BUSCLK                  |
| Interrupt       | Zero/Load Event Enabled |
| External Output | None                    |

中断函数中只设置标志：

```c
volatile bool g_control_tick = false;

void TIMG0_IRQHandler(void)
{
    switch (DL_TimerG_getPendingInterrupt(CONTROL_TICK_INST)) {
    case DL_TIMER_IIDX_ZERO:
        g_control_tick = true;
        break;

    default:
        break;
    }
}
```

主循环：

```c
while (1) {
    uart_process();

    if (g_control_tick) {
        g_control_tick = false;
        motor_control_update();
    }
}
```

这样I²C读取不会阻塞定时器ISR。

### 后续提高频率

第一版：

```text
控制循环：1kHz
PWM载波：20kHz
```

跑稳以后可以把控制循环提高到2kHz。AS5600自身采样和滤波存在延迟，没必要一开始追求10kHz位置循环。AS5600的典型内部采样间隔为150μs，快速滤波设置下响应可缩短到约286μs。

---

# 三、SysConfig完成后的检查清单

点击保存，让SysConfig重新生成代码。检查：

```text
ti_msp_dl_config.h
ti_msp_dl_config.c
```

你应该能找到类似符号：

```c
FOC_PWM_INST
FOC_PWM_INST_CC_0_INDEX
FOC_PWM_INST_CC_1_INDEX
FOC_PWM_INST_CC_3_INDEX

AS5600_I2C_INST

CMD_UART_INST
CMD_UART_INST_INT_IRQN

CONTROL_TICK_INST
CONTROL_TICK_INST_INT_IRQN

FOC_EN_PORT
FOC_EN_PIN

FOC_NFAULT_PORT
FOC_NFAULT_PIN
```

实际宏名可能因为SDK版本和实例名称略有不同，因此代码必须以生成的`ti_msp_dl_config.h`为准。

---

# 四、闭环控制程序应该采用的结构

## 1. 三相电压生成

没有相电流采样，采用：

```text
Ud = 0
Uq = 速度控制器输出
```

根据电角度产生三相电压：

[
\theta_e=p\theta_m+\theta_0
]

其中：

* (p)：电机极对数；
* (\theta_m)：AS5600机械角；
* (\theta_0)：电角度零偏。

正弦PWM可写为：

```c
u_alpha = -uq * sinf(electrical_angle);
u_beta  =  uq * cosf(electrical_angle);

ua = u_alpha;
ub = -0.5f * u_alpha + 0.8660254f * u_beta;
uc = -0.5f * u_alpha - 0.8660254f * u_beta;
```

然后加上母线中点：

```c
duty_a = 0.5f + ua / bus_voltage;
duty_b = 0.5f + ub / bus_voltage;
duty_c = 0.5f + uc / bus_voltage;
```

最后限幅到安全范围，比如：

```c
0.02f～0.98f
```

---

## 2. 位置闭环建议用串级控制

不要直接：

```c
Uq = Kp * angle_error;
```

建议：

```text
目标角度
   ↓
位置P控制器
   ↓
目标速度
   ↓
速度PI控制器
   ↓
Uq
   ↓
电压模式FOC
```

公式：

```c
velocity_target =
    position_kp * position_error;

uq =
    velocity_kp * velocity_error
    + velocity_ki * velocity_integral;
```

并设置：

```c
velocity_target = clamp(velocity_target, -velocity_limit, velocity_limit);
uq = clamp(uq, -voltage_limit, voltage_limit);
```

初始调试建议：

```text
母线电压：按实际填写，例如8V
FOC电压限制：0.3～0.5V
角度范围：±10°
目标速度限制：1～2rad/s
```

电机能正常响应后，再逐渐提高电压限制。

---

# 五、UART命令协议建议

第一版使用ASCII，方便串口助手测试。

```text
EN 1
EN 0
CAL
ZERO
A 10.0
A -5.0
STATUS
KP 8.0
VP 0.5
VI 1.0
VLIM 0.5
PP 7
DIR 1
```

含义：

| 命令         | 功能              |
| ---------- | --------------- |
| `EN 1`     | 使能电机            |
| `EN 0`     | 关闭电机            |
| `CAL`      | 电角度对齐           |
| `ZERO`     | 当前机械位置设为0°      |
| `A 10.0`   | 转到相对零点+10°并保持   |
| `STATUS`   | 返回角度、速度、Uq、故障状态 |
| `KP x`     | 设置位置环Kp         |
| `VP x`     | 设置速度环Kp         |
| `VI x`     | 设置速度环Ki         |
| `VLIM x`   | 设置最大Uq电压        |
| `PP n`     | 设置电机极对数         |
| `DIR 1/-1` | 设置传感器方向         |

第一阶段测试顺序：

```text
STATUS
CAL
ZERO
EN 1
A 3
A -3
A 0
```

不要第一次直接发送：

```text
A 90
```

---

# 六、给Codex的完整Prompt

把下面整段复制给Codex。最好在你的CCS工程根目录中使用，让Codex能读取`.syscfg`和生成的`ti_msp_dl_config.h`。

```text
你现在位于一个基于TI MSPM0G3507“地猛星”开发板的Code Composer Studio工程中。

目标：
为一台2204无刷云台电机编写带AS5600绝对磁编码器反馈的闭环位置控制代码。电机通过SimpleFOC Mini v1.0三相驱动板驱动。通过UART发送目标角度，例如“A 10.0\n”，电机应转到相对零点+10度并保持。

重要限制：
1. 不使用Arduino框架。
2. 不使用SimpleFOC Arduino库。
3. 使用TI MSPM0 SDK DriverLib和SysConfig生成的ti_msp_dl_config.c/.h。
4. 不得手动修改ti_msp_dl_config.c或ti_msp_dl_config.h。
5. 当前硬件没有相电流采样，因此实现“带传感器的电压模式FOC”，不是电流闭环FOC。
6. 首先检查工程现有文件、.syscfg和ti_msp_dl_config.h，使用实际生成的外设宏名称，不得凭空编造宏。
7. 代码必须能在TI Arm Clang/MSPM0 SDK工程中编译，不使用动态内存。

硬件与SysConfig假设：
- MCU：MSPM0G3507。
- SimpleFOC Mini IN1：PA21，TIMA0_C0。
- SimpleFOC Mini IN2：PA22，TIMA0_C1。
- SimpleFOC Mini IN3：PA23，TIMA0_C3。
- SimpleFOC Mini EN：PA24，GPIO输出，上电初始为低。
- SimpleFOC Mini nFT：PA25，GPIO输入，上拉，低电平故障；若工程未配置PA25，则通过编译宏允许关闭该功能。
- AS5600使用I2C0：
  - SDA：PA0。
  - SCL：PA1。
  - 7位地址：0x36。
  - STATUS寄存器：0x0B。
  - RAW_ANGLE寄存器：0x0C和0x0D。
- UART1：
  - TX：PA8。
  - RX：PA9。
  - 115200，8N1。
- TIMA0输出三路同步PWM：
  - 中心对齐。
  - 20kHz。
  - CC0、CC1、CC3。
- TIMG0产生1kHz控制节拍中断。
- TIMG0中断只设置控制标志，不允许在中断中进行阻塞I2C操作。
- UART RX使用中断和环形缓冲区。
- I2C第一版采用DriverLib轮询/阻塞传输，但必须带超时，禁止永久死等。

首先完成以下工作：
1. 检查.syscfg和ti_msp_dl_config.h。
2. 列出工程中实际生成的外设实例宏、CC通道宏、IRQ名称、GPIO端口和引脚宏。
3. 如果SysConfig配置与上述假设不一致，优先适配现有配置，并在README中指出差异。
4. 不要重新生成或重写SysConfig生成文件。

请创建以下文件，必要时可根据现有项目结构调整命名：

as5600.h
as5600.c
foc_pwm.h
foc_pwm.c
motor_control.h
motor_control.c
uart_command.h
uart_command.c
app_config.h
main.c
README_MOTOR_CONTROL.md

一、AS5600驱动要求

实现：
bool as5600_init(void);
bool as5600_read_status(uint8_t *status);
bool as5600_read_raw(uint16_t *raw);
bool as5600_update(float dt);
float as5600_get_angle_rad(void);
float as5600_get_unwrapped_angle_rad(void);
float as5600_get_velocity_rad_s(void);
bool as5600_magnet_ok(void);

要求：
- RAW_ANGLE只保留低12位。
- 正确处理0与4095之间的跨零跳变。
- 维护单圈角度和展开后的连续机械角。
- 速度使用角度差分加一阶低通滤波。
- STATUS中的MD位必须为1；ML或MH出现时报告警告。
- 连续I2C失败达到阈值后关闭电机。
- I2C读写必须有超时和错误恢复。
- 不允许在ISR中调用AS5600阻塞读取。
- 代码注释中说明AS5600地址使用7位0x36，不要左移。

二、三相PWM和电压模式FOC

实现：
void foc_pwm_init(void);
void foc_pwm_enable(bool enable);
void foc_pwm_set_phase_duty(float da, float db, float dc);
void foc_set_phase_voltage(float uq, float electrical_angle);
void foc_pwm_stop(void);

要求：
- 初始化时EN保持低。
- 三相初始占空比设为50%。
- 三个CC比较寄存器尽可能使用同步/影子更新方式。
- 根据SysConfig生成的PWM周期值计算比较值，不硬编码80MHz或固定LOAD值。
- duty限制在0.02到0.98。
- 实现正弦PWM：
  u_alpha = -uq * sin(theta_e)
  u_beta  =  uq * cos(theta_e)
  ua = u_alpha
  ub = -0.5*u_alpha + sqrt(3)/2*u_beta
  uc = -0.5*u_alpha - sqrt(3)/2*u_beta
  duty = 0.5 + phase_voltage / bus_voltage
- 提供normalize_angle_0_2pi()函数。
- 所有三相占空比必须在同一个控制周期内统一更新。
- 不配置或使用死区，因为SimpleFOC Mini使用DRV8313集成三相驱动。
- 电机相序必须集中定义，方便通过宏交换B/C相，而不是散落在代码中。

三、电机参数和安全参数

在app_config.h中提供可修改默认值：

#define MOTOR_POLE_PAIRS       7
#define SENSOR_DIRECTION       1
#define MOTOR_PHASE_ORDER      0
#define BUS_VOLTAGE_V          8.0f
#define VOLTAGE_LIMIT_V        0.4f
#define ALIGN_VOLTAGE_V        0.35f
#define POSITION_LIMIT_DEG     20.0f
#define VELOCITY_LIMIT_RAD_S   2.0f
#define CONTROL_FREQUENCY_HZ   1000.0f

必须明确注释：
- MOTOR_POLE_PAIRS只是初始值，用户必须核实。
- 总磁极数除以2等于极对数。
- 初次测试电压限制必须很低。
- BUS_VOLTAGE_V必须与实际驱动板供电一致。

四、电角度对齐

实现一个安全的motor_calibrate()：

bool motor_calibrate(void);

要求：
- 只有AS5600磁铁状态正常时才允许校准。
- 校准开始前目标角度清零、积分器清零。
- 拉高EN后使用很低的ALIGN_VOLTAGE_V施加固定电压矢量，使转子对齐。
- 等待约500到1000ms。
- 读取机械角并计算electrical_zero_offset。
- 校准过程需要考虑SENSOR_DIRECTION。
- 校准结束后将Uq置零，但可以保持驱动器待命。
- 通过UART打印校准得到的：
  - mechanical angle
  - electrical zero offset
  - pole pairs
  - sensor direction
- 如果电机剧烈抖动、编码器不变化、nFT拉低或I2C失败，立即EN置低并返回失败。
- 将电角度零偏保存在RAM变量中即可，第一版不要求写Flash。
- 将公式、相位约定和正负方向写清楚，保证foc_set_phase_voltage()与校准公式使用同一套符号定义。
- 如果无法完全自动判断传感器方向，提供UART命令DIR 1和DIR -1，由用户手动切换。

五、位置闭环控制

使用串级结构：

位置P环：
velocity_target = position_kp * position_error

速度PI环：
velocity_error = velocity_target - measured_velocity
uq = velocity_kp * velocity_error
   + velocity_ki * velocity_integral

要求：
- 目标角度以相对ZERO位置的度数表示。
- 内部统一使用rad和rad/s。
- 使用展开后的机械角。
- velocity_target限幅。
- uq限幅到±voltage_limit。
- 实现积分限幅和抗积分饱和。
- 当电机未校准、未使能或传感器故障时，uq必须为0。
- 目标角度限制在±POSITION_LIMIT_DEG。
- 提供目标角度变化率限制，避免目标突然跳变。
- 提供停止和复位控制器状态的函数。
- 默认参数设置保守，确保第一次运行不会猛烈旋转。
- 控制循环运行在主循环中，由1kHz tick标志触发。
- 1kHz定时器ISR中只设置volatile标志。

推荐接口：

typedef struct {
    bool enabled;
    bool calibrated;
    bool sensor_ok;
    bool driver_fault;
    float target_angle_rad;
    float commanded_angle_rad;
    float measured_angle_rad;
    float measured_velocity_rad_s;
    float velocity_target_rad_s;
    float uq_command_v;
    float electrical_zero_offset;
    int pole_pairs;
    int sensor_direction;
} MotorControlState;

void motor_control_init(void);
void motor_control_update(float dt);
bool motor_control_calibrate(void);
void motor_control_enable(bool enable);
void motor_control_set_target_deg(float deg);
void motor_control_zero_here(void);
void motor_control_reset(void);
const MotorControlState *motor_control_get_state(void);

六、UART命令

实现基于换行符的ASCII命令解析器，使用固定长度缓冲区，不使用malloc、strtok线程不安全形式或无限长度字符串。

必须支持：

EN 1
EN 0
CAL
ZERO
A 10.0
A -5.0
STATUS
KP 8.0
VP 0.5
VI 1.0
VLIM 0.4
VELIM 2.0
PP 7
DIR 1
DIR -1
HELP

行为：
- “A 10.0”表示相对ZERO点+10度并保持。
- 对所有参数做范围检查。
- 命令成功返回“OK ...”。
- 命令错误返回“ERR ...”。
- STATUS返回：
  enable
  calibrated
  target_deg
  actual_deg
  velocity_rad_s
  uq
  raw_angle
  magnet_status
  driver_fault
  pole_pairs
  sensor_direction
  voltage_limit
- UART RX ISR只接收字节并放入环形缓冲区。
- 命令解析在main循环执行。
- TX可以采用轮询发送，但必须有超时。
- 防止printf浮点支持问题；如工具链默认不支持浮点printf，请实现轻量格式化，或在README中说明如何启用。

七、故障和安全

满足任一条件时立即：
1. 三相占空比回到50%或安全关闭状态。
2. EN拉低。
3. 清空速度PI积分。
4. 标记fault。

故障条件：
- AS5600连续读取失败。
- AS5600 MD位为0。
- nFT为低。
- 控制循环超时。
- 参数为NaN或无穷。
- PWM duty计算越界。
- UART命令要求关闭。
- 校准失败。

提供CLEAR命令或EN 0后重新初始化的清故障流程，但不能自动忽略硬件故障。

八、main.c流程

main应当大致为：

SYSCFG_DL_init();
初始化GPIO安全状态；
EN保持低；
初始化三相PWM并设置50%；
初始化I2C/AS5600；
初始化UART和RX中断；
初始化1kHz控制定时器中断；
开启全局中断；
打印启动信息；
进入while(1)：
    uart_command_process();
    如果control_tick：
        清tick；
        as5600_update(0.001f);
        motor_control_update(0.001f);
    处理故障和低优先级状态输出；

不要自动校准并高速启动。
上电后必须等待用户发送CAL和EN 1。

九、编译与代码质量

- 使用stdint.h、stdbool.h、math.h。
- 所有共享ISR变量使用volatile。
- 避免在ISR中执行复杂运算。
- 避免魔法数字。
- 编译无warning。
- 对DriverLib返回值和I2C状态进行检查。
- 对SDK版本差异进行兼容处理时，应集中放到一个适配层，不要在各文件散布大量#ifdef。
- 不修改生成文件。
- 代码注释使用中文或清晰英文均可。
- 如果工程现有代码已经封装UART/I2C/GPIO，优先复用，不要重复创建不兼容接口。

十、README内容

README_MOTOR_CONTROL.md必须包含：
1. 实际使用的引脚表。
2. SimpleFOC Mini v1.0接线提示。
3. 红黑黄三根电机线全部是三相线，黑线不是GND。
4. AS5600供电和I2C接线。
5. 所有模块共地要求。
6. 初次测试使用8V限流电源。
7. 建议初始限流0.3到0.5A。
8. 初次测试命令顺序：
   STATUS
   CAL
   ZERO
   EN 1
   A 3
   A -3
   A 0
9. 如何确定极对数。
10. 电机抖动时依次检查：
    - 极对数
    - 编码器方向
    - 电角度零偏
    - 三相相序
    - 磁铁同心度
    - PWM通道映射
11. 如何逐步调位置Kp、速度Kp和速度Ki。
12. 明确说明该方案是电压模式FOC，没有相电流闭环。

最后：
先给出你对现有工程和SysConfig生成宏的检查结果，再创建或修改文件。完成后执行构建。如果无法构建，准确列出编译错误、对应文件和建议修复，不要声称已经成功。
```

这份Prompt有一个关键点：**让Codex先读取你工程实际生成的宏**。否则它很容易写出看似正确、但与当前MSPM0 SDK生成名称不匹配的代码。

实际第一次上电时，先只执行：

```text
STATUS
```

确认AS5600角度随手转电机连续变化，磁铁状态正常。随后把驱动电源限制在8V、0.3～0.5A，电压输出上限从0.3～0.4V开始，再进行`CAL`和±3°小角度测试。

[1]: https://docs.simplefoc.com/mini_v1_connect_hardware "Mini v1 | Arduino-FOC"
[2]: https://www.ti.com/lit/ds/symlink/mspm0g3507.pdf "MSPM0G350x Mixed-Signal Microcontrollers With CAN-FD Interface datasheet (Rev. C)"
[3]: https://software-dl.ti.com/msp430/esd/MSPM0-SDK/2_06_00_05/release_notes_mspm0_sdk_2_06_00_05.html "MSPM0 SDK Release Notes"
