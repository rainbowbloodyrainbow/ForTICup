# Ball and Beam 电机闭环控制

本工程面向 MSPM0G3507“地猛星”开发板、SimpleFOC Mini v1.0、
AS5600 和 2204 无刷云台电机。控制方式是带编码器反馈的电压模式
FOC：没有相电流采样，因此不是电流闭环 FOC。

## 实际 SysConfig 引脚

| 功能 | MCU 引脚 | 外设 |
| --- | --- | --- |
| SimpleFOC IN1 | PA21 | TIMA0_CCP0 |
| SimpleFOC IN2 | PA22 | TIMA0_CCP1 |
| SimpleFOC IN3 | PA23 | TIMA0_CCP3 |
| SimpleFOC EN | PA24 | GPIO 输出，初始低 |
| SimpleFOC nFT | PA25 | GPIO 输入，上拉，低有效 |
| AS5600 SDA | PA0 | I2C0_SDA |
| AS5600 SCL | PA1 | I2C0_SCL |
| 命令串口 TX | PA8 | UART1_TX |
| 命令串口 RX | PA9 | UART1_RX |
| 控制节拍 | 无外部引脚 | TIMG0，1 kHz |

`ballandbeam.syscfg` 已使用 MSPM0 SDK 2.11.00.07 和 SysConfig 1.28
命令行工具校验。当前默认 BUSCLK 为 32 MHz；TIMA0 配置为 20 kHz
中心对齐 PWM，三路 CC 使用 ZERO 事件影子更新。

## 接线和供电

SimpleFOC Mini v1.0 控制排针应以板上丝印为准，不能直接照搬 v1.1
排针顺序。PA21、PA22、PA23 分别连接 IN1、IN2、IN3，PA24 连接 EN，
PA25 可连接 nFT。

电机红、黑、黄三根线全部是三相线，分别接 M1、M2、M3；黑线不是
GND。若方向或相序错误，先通过 `DIR` 检查传感器方向，再考虑交换
任意两根相线或修改 `MOTOR_PHASE_ORDER`。

AS5600 使用 3.3 V 供电：

- VCC -> 3V3
- GND -> GND
- SDA -> PA0
- SCL -> PA1
- SDA、SCL 各用约 4.7 kΩ 上拉到 3.3 V

动力电源负极、SimpleFOC Mini GND、地猛星 GND、AS5600 GND 和串口
发送端 GND 必须共地，但电机动力电流不要流过编码器或 MCU 的细地线。
SimpleFOC Mini 的 3V3 不用于给地猛星供电。

初次上电建议使用 8 V 限流电源，限流从 0.3～0.5 A 开始。确认
`BUS_VOLTAGE_V` 与实测供电一致，并将电压限制保持在默认 0.4 V
附近；不要一开始提高输出限制。

## 构建

```text
cd Ball_and_Beam
make
```

构建会先从 `ballandbeam.syscfg` 重新生成 `ti_generated`，然后输出
`firmware.elf` 和 `firmware.hex`。不要手工修改 `ti_generated` 中的
文件。

## UART 协议

UART1 使用 PA8 TX、PA9 RX，115200、8N1。上位机 TX 接 PA9，上位机
RX 接 PA8，并连接 GND。命令以换行结尾：

```text
EN 1
EN 0
CAL
ZERO
A 3.0
A -3.0
STATUS
KP 4.0
VP 0.2
VI 0.8
VLIM 0.4
VELIM 2.0
PP 7
DIR 1
DIR -1
CLEAR
HELP
```

`A 3.0` 表示相对 ZERO 点转到 +3° 并保持，目标被限制在
`±POSITION_LIMIT_DEG`。改变 `PP` 或 `DIR` 后必须重新 `CAL`。

## 首次测试顺序

先不接电机动力电源，手动转动电机并多次发送 `STATUS`，确认
`raw_angle` 在 0～4095 之间连续变化，且 `magnet_status` 的 MD 位
（0x20）为 1。ML（0x10）或 MH（0x08）表示磁场过弱或过强，需要
调整磁铁距离。

确认传感器后，接入 8 V 限流电源并依次发送：

```text
STATUS
CAL
ZERO
EN 1
A 3
A -3
A 0
EN 0
```

上电不会自动校准或使能。任何传感器连续读失败、MD 丢失、nFT
拉低、控制循环严重超时或非法运算都会让三相回到 50% 并拉低 EN。
排除硬件问题后，在电机关闭状态发送 `CLEAR`，再重新校准。

## 参数确认与调试

`MOTOR_POLE_PAIRS` 默认 7 只用于让工程可编译，不代表这台 2204
一定是 7 极对。数出转子的总磁极数并除以 2，或用低压扫描辨识，
随后用 `PP n` 设置并重新校准。

电机抖动时依次检查：

1. 极对数是否正确。
2. 编码器方向是否正确，可尝试 `DIR -1` 后重新 `CAL`。
3. 电角度零偏是否由当前接线重新校准。
4. 三相相序和 PA21/PA22/PA23 通道映射。
5. 磁铁是否同心、距离是否合适。
6. PWM 引脚是否确实连接到 IN1/IN2/IN3。

调参时始终保持很小的 `VLIM`。先把 `VI` 设为 0，逐步提高 `VP`
直到速度响应明确但不振荡；再逐步提高 `KP` 改善位置响应；最后只
增加少量 `VI` 消除静差。每次只改一个参数，并从 ±3° 小角度开始。
