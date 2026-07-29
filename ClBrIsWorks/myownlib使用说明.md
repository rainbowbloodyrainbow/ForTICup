# myownlib 使用说明

`myownlib` 是基于 TI MSPM0 DriverLib 编写的个人应用库，用来把常用的底层外设操作封装成直观、可复用的接口。

> 正式竞赛工程已经改为根目录唯一的 `main.c`、`Makefile`、`all.syscfg`
> 和 `00_generated/`。`Examples/` 只保留硬件学习记录，不参与根工程构建。
> 当前正式依赖链为：
>
> ```text
> 01_platform/adc + output + system_time
> → 02_device/line_sensor + motor + servo + hc05
> → 03_algorithm/pid
> → 04_control/line_control
> → 05_robot/chassis
> → 07_application/application
> ```
>
> 八路循迹、电机、舵机及 PID 的上车可调参数集中在
> `myownlib/07_application/application/application_config.h`。本文后面的
> Example 接入内容记录的是重构前的学习工程路径，正式工程以根 Makefile
> 为准。

## 项目概览

### 适用环境

当前代码面向以下开发环境：

- MCU：MSPM0G3507。
- SDK：MSPM0 SDK 2.11.00.07。
- 配置工具：TI SysConfig。
- 编译器：ARM GNU Toolchain。

### 当前模块

目前已经完成 `output`、`hc05`、`adc` 和 `mpu6050` 模块：

```text
modules/
├── output/
│   ├── output.c
│   └── output.h
├── hc05/
│   ├── hc05.c
│   └── hc05.h
├── adc/
│   ├── adc.c
│   └── adc.h
└── mpu6050/
    ├── mpu6050.c
    └── mpu6050.h
```

`output` 模块提供两类能力：

- `DigitalOutput`：普通 GPIO 高低电平输出。
- `PwmOutput`：基于 TIMA/TIMG 的硬件 PWM 输出。

`hc05` 模块提供：

- 阻塞式字节、缓冲区、字符串和整数发送。
- UART RX 中断接收和循环队列。

`adc` 模块提供：

- 阻塞式单通道 ADC 原始值读取。
- 12 位 ADC 原始值到毫伏的换算。

`mpu6050` 模块提供：

- 非阻塞初始化和 `WHO_AM_I` 检查。
- I²C 中断状态机和错误恢复。
- 14 字节六轴原始数据读取和解析。
- 当前 ±250 dps 量程下的角速度换算。

后续新增模块时，应继续放在 `modules/` 下，并在本文档的“模块说明”中增加相应章节。

### 库与 SysConfig 的分工

`myownlib` 只负责操作已经配置好的外设，不代替 SysConfig 分配硬件资源。

每个应用工程仍然需要在自己的 `.syscfg` 文件中配置：

- GPIO 的端口、引脚、输出方向和初始电平。
- PWM 使用的定时器、通道、周期、时钟和输出引脚。
- ADC 的实例、输入通道、参考电压来源、采样时间和时钟。
- I²C 的控制器实例、总线速度、SDA/SCL 引脚和中断。
- 该工程使用的其他外设资源。

程序启动后必须先执行：

```c
SYSCFG_DL_init();
```

完成硬件初始化后，才能调用应用库接口。

不要直接修改 SysConfig 自动生成的 `ti_msp_dl_config.c/.h`。

### 仓库结构

当前仓库采用以下布局：

```text
MSPM0/
├── Examples/
│   ├── 01_LEDtest/
│   ├── 02_hc05/
│   ├── 03_ADCread/
│   ├── 04_ssd1306/
│   └── 05_mpu6050/
├── modules/
│   ├── output/
│   │   ├── output.c
│   │   └── output.h
│   ├── hc05/
│   │   ├── hc05.c
│   │   └── hc05.h
│   ├── adc/
│   │   ├── adc.c
│   │   └── adc.h
│   └── mpu6050/
│       ├── mpu6050.c
│       └── mpu6050.h
└── myownlib使用说明.md
```

`Examples/` 保存可以独立生成固件的应用工程，`modules/` 保存由多个应用工程直接引用的公共实现。

## 快速接入

以下示例假设新工程位于：

```text
Examples/YourProject/
```

下面同时展示当前所有模块的接入方法。实际工程只需添加自己使用的模块，不必把所有模块都加入 Makefile。

### 在 Makefile 中添加头文件路径

在 `INCLUDES` 中加入：

```make
-I../../modules/output
-I../../modules/hc05
-I../../modules/adc
-I../../modules/mpu6050
```

完整形式例如：

```make
INCLUDES = \
	-I$(GEN_DIR) \
	-I../../modules/output \
	-I../../modules/hc05 \
	-I../../modules/adc \
	-I../../modules/mpu6050 \
	-I$(SDK_ROOT)/source \
	-I$(SDK_ROOT)/source/third_party/CMSIS/Core/Include
```

### 在 Makefile 中添加模块源文件

在编译源文件列表中加入：

```make
../../modules/output/output.c
../../modules/hc05/hc05.c
../../modules/adc/adc.c
../../modules/mpu6050/mpu6050.c
```

例如：

```make
$(CC) $(CFLAGS) $(INCLUDES) \
	main.c \
	../../modules/output/output.c \
	../../modules/hc05/hc05.c \
	../../modules/adc/adc.c \
	../../modules/mpu6050/mpu6050.c \
	$(GEN_DIR)/ti_msp_dl_config.c \
	$(STARTUP_FILE) \
	$(DRIVERLIB) \
	$(LDFLAGS) \
	-o firmware.elf
```

如果工程不在 `Examples/` 的下一层目录中，需要根据实际位置调整相对路径。

### 在应用代码中包含模块

```c
#include "ti_msp_dl_config.h"
#include "adc.h"
#include "hc05.h"
#include "mpu6050.h"
#include "output.h"
```

最小使用流程为：

```c
int main(void)
{
    SYSCFG_DL_init();

    /* 在这里调用 myownlib 接口 */

    while (1) {
    }
}
```

## 模块说明

### output

`output` 是通用输出模块，不局限于 LED。它可以操作 LED、蜂鸣器控制端、继电器控制端、电机驱动器逻辑输入等低功率逻辑信号。

GPIO 和 PWM 引脚只能输出逻辑电平，不能直接为电机等大功率负载供电。

#### DigitalOutput

##### 接口

```c
void DigitalOutput_High(GPIO_Regs *port, uint32_t pin);
void DigitalOutput_Low(GPIO_Regs *port, uint32_t pin);
void DigitalOutput_Write(GPIO_Regs *port, uint32_t pin, bool high);
void DigitalOutput_Toggle(GPIO_Regs *port, uint32_t pin);
```

| 接口 | 功能 |
| --- | --- |
| `DigitalOutput_High()` | 输出高电平 |
| `DigitalOutput_Low()` | 输出低电平 |
| `DigitalOutput_Write()` | 根据 `true` 或 `false` 输出高、低电平 |
| `DigitalOutput_Toggle()` | 翻转当前输出电平 |

模块使用 `High/Low`，而不是 `On/Off`。这是因为某些 LED、继电器和驱动器采用低电平有效，低电平不一定代表设备关闭。

##### SysConfig 要求

使用前应在 SysConfig 中将目标引脚配置为：

- GPIO 数字输出。
- 合适的初始电平。
- 与应用代码一致的端口和引脚名称。

##### 基本用法

假设 SysConfig 中的 GPIO 分组名为 `USER`，引脚名为 `LED1`：

```c
DigitalOutput_High(USER_PORT, USER_LED1_PIN);
DigitalOutput_Low(USER_PORT, USER_LED1_PIN);
DigitalOutput_Toggle(USER_PORT, USER_LED1_PIN);
```

也可以根据条件直接写入：

```c
bool outputHigh = true;

DigitalOutput_Write(USER_PORT, USER_LED1_PIN, outputHigh);
```

同一个接口可以操作任意数量的已配置引脚，不需要为 LED1、LED2 等分别编写函数：

```c
DigitalOutput_Toggle(USER_PORT, USER_LED1_PIN);
DigitalOutput_Toggle(USER_PORT, USER_LED2_PIN);
```

如果输出位于不同 GPIO 端口，也可以分别传入：

```c
DigitalOutput_High(GPIOA, DL_GPIO_PIN_7);
DigitalOutput_High(GPIOB, DL_GPIO_PIN_2);
```

实际工程中优先使用 SysConfig 生成的端口和引脚宏，避免在应用代码中硬编码引脚。

#### PwmOutput

##### 接口

```c
void PwmOutput_Start(GPTIMER_Regs *timer);
void PwmOutput_Stop(GPTIMER_Regs *timer);

void PwmOutput_SetDuty(
    GPTIMER_Regs *timer,
    DL_TIMER_CC_INDEX channel,
    uint16_t duty);
```

##### 占空比单位

PWM 占空比采用 `0～1000` 的千分比：

| 参数值 | 实际占空比 |
| ---: | ---: |
| `0` | 0% |
| `250` | 25% |
| `500` | 50% |
| `750` | 75% |
| `1000` | 100% |

库中定义了：

```c
#define PWM_OUTPUT_DUTY_MAX (1000U)
```

传入值超过 `1000` 时，模块会按 `1000` 处理。

##### SysConfig 要求

当前 `PwmOutput_SetDuty()` 按以下配置换算高电平占空比：

- PWM Mode：Edge-aligned Down Counting。
- 输出未反相。
- 高电平占空比由比较值控制。
- 定时器周期、时钟、通道和引脚已经由 SysConfig 配置。

如果启用输出反相，最终引脚上的亮暗或高低占空比关系会颠倒。

##### TimerG 示例

假设 SysConfig 将 PWM 实例命名为 `PWM_0`，并使用 TimerG 的 Capture/Compare 0：

```c
PwmOutput_SetDuty(
    PWM_0_INST,
    DL_TIMERG_CAPTURE_COMPARE_0_INDEX,
    500U);

PwmOutput_Start(PWM_0_INST);
```

上述代码输出约 50% 的高电平占空比。

##### TimerA 示例

如果 SysConfig 分配的是 TimerA，应使用对应的 TimerA 通道宏：

```c
PwmOutput_SetDuty(
    PWM_0_INST,
    DL_TIMERA_CAPTURE_COMPARE_0_INDEX,
    750U);

PwmOutput_Start(PWM_0_INST);
```

具体使用 `DL_TIMERA_*` 还是 `DL_TIMERG_*`，由 SysConfig 最终分配的定时器类型决定。

##### 动态修改占空比

定时器启动后，可以继续修改占空比：

```c
PwmOutput_SetDuty(
    PWM_0_INST,
    DL_TIMERG_CAPTURE_COMPARE_0_INDEX,
    200U);
```

新值立即生效还是在周期边界生效，取决于 SysConfig 中 Capture/Compare 的更新方式。

##### 多通道 PWM

一个定时器可以通过多个 Capture/Compare 通道输出多路 PWM。多个通道共享计数器、周期和频率，但占空比可以分别设置。

`Examples/01_LEDtest` 的实测结构为：

```text
TIMG0，共用 1 kHz 周期
├── CCP0 → PA12
└── CCP1 → PA13
```

两个通道先分别设置占空比，然后只启动一次定时器：

```c
PwmOutput_SetDuty(
    LED_PWM_INST,
    GPIO_LED_PWM_C0_IDX,
    250U);

PwmOutput_SetDuty(
    LED_PWM_INST,
    GPIO_LED_PWM_C1_IDX,
    750U);

PwmOutput_Start(LED_PWM_INST);
```

上述代码让 PA12 输出 25%、PA13 输出 75%，两路频率均为 1 kHz。

这里优先使用 SysConfig 生成的 `GPIO_LED_PWM_C0_IDX` 和 `GPIO_LED_PWM_C1_IDX`，避免在应用代码中再次硬编码通道编号。

### hc05

HC-05 在正常工作模式下是透明串口设备。模块只负责操作已经配置好的 UART，不负责分配 UART 实例、引脚或波特率。

#### 发送接口

```c
void HC05_SendByte(UART_Regs *uart, uint8_t data);
void HC05_SendBuffer(
    UART_Regs *uart, const uint8_t *data, uint32_t length);
void HC05_SendString(UART_Regs *uart, const char *text);

void HC05_SendUint32(UART_Regs *uart, uint32_t value);
void HC05_SendInt32(UART_Regs *uart, int32_t value);
void HC05_SendHex8(UART_Regs *uart, uint8_t value);
```

`HC05_SendBuffer()` 按指定长度发送，可以发送包含 `0x00` 的二进制数据；`HC05_SendString()` 在遇到字符串结尾 `'\0'` 时停止。

数字发送接口不依赖 `printf`：

- `HC05_SendUint32()`：发送无符号十进制整数。
- `HC05_SendInt32()`：发送带符号十进制整数。
- `HC05_SendHex8()`：发送两位大写十六进制数，不自动添加 `0x`。

当前发送接口使用 DriverLib 阻塞发送。数据发送完成前 CPU 会停留在函数内，适合提示信息和低频调试输出。

#### 接收接口

```c
void HC05_ResetReceiver(void);
void HC05_HandleRxInterrupt(UART_Regs *uart);
bool HC05_DataAvailable(void);
bool HC05_ReadByte(uint8_t *data);
bool HC05_RxOverflowed(void);
void HC05_ClearRxOverflow(void);
```

模块内部使用一个 64 字节数组实现循环队列。为区分队列为空和已满，实际最多保存 63 个尚未处理的字节；队列已满时丢弃新收到的字节并设置溢出标志。

循环队列只对应一路 HC-05 接收通道。中断负责写入队列，主循环负责读取队列。

#### SysConfig 要求

应用工程应在 SysConfig 中完成：

- 添加 UART，并启用发送和接收。
- 配置与 HC-05 一致的波特率、数据位、停止位和校验位。
- 分配 TX、RX 引脚。
- 启用 UART RX 中断。

当前示例使用 UART1、115200、8-N-1、PA8 TX 和 PA9 RX。

#### 中断接收流程

硬件初始化后，先清空软件接收队列，再启用 CPU 侧 UART 中断：

```c
SYSCFG_DL_init();
HC05_ResetReceiver();

NVIC_ClearPendingIRQ(HC05_UART_INST_INT_IRQN);
NVIC_EnableIRQ(HC05_UART_INST_INT_IRQN);
```

应用工程必须保留自己的中断入口，并把接收处理交给模块：

```c
void HC05_UART_INST_IRQHandler(void)
{
    HC05_HandleRxInterrupt(HC05_UART_INST);
}
```

主循环通过 `HC05_ReadByte()` 取出数据：

```c
uint8_t receivedByte;

if (HC05_ReadByte(&receivedByte)) {
    HC05_SendByte(HC05_UART_INST, receivedByte);
}
```

`HC05_ReadByte()` 在成功取出一个字节时返回 `true`，队列为空时返回 `false`，因此通常可以直接放在 `if` 条件中。`HC05_DataAvailable()` 适合只想查询队列状态、暂时不取数据的场景。

启用这种中断接收方式后，不要再使用阻塞式 UART 接收函数，否则中断和主循环会争抢硬件接收数据。

### adc

`adc` 模块负责操作已经由 SysConfig 配置好的 ADC12 外设。当前接口面向使用 `ADCMEM0` 的阻塞式单通道采样。

#### 接口

```c
uint16_t ADC_ReadRaw(ADC12_Regs *adc);

uint32_t ADC_RawToMillivolts(
    uint16_t rawValue,
    uint32_t referenceMillivolts);
```

`ADC_ReadRaw()` 启动一次转换，等待 `ADCMEM0` 装载结果，清除完成标志并返回 12 位原始值。

`ADC_RawToMillivolts()` 使用以下关系换算：

```text
电压（mV）= ADC 原始值 × 参考电压（mV）÷ 4095
```

#### SysConfig 要求

应用工程应在 SysConfig 中完成：

- 添加并分配 ADC12 实例。
- 为 `ADCMEM0` 选择模拟输入通道。
- 配置参考电压来源。
- 配置采样时钟、分频和采样时间。
- 根据应用选择合适的电源模式。

调用模块接口前必须先执行 `SYSCFG_DL_init()`。

#### 基本用法

```c
uint16_t rawValue;
uint32_t voltageMv;

rawValue = ADC_ReadRaw(ADC12_0_INST);
voltageMv = ADC_RawToMillivolts(rawValue, 3300U);
```

对于光电循迹等阈值判断，通常直接使用 `rawValue`。毫伏换算主要用于调试显示和数据记录。

当前读取函数会阻塞到转换完成，不负责采样周期、多路调度、滤波、传感器标定或阈值判断；这些功能应由上层传感器模块或应用程序实现。

### mpu6050

`mpu6050` 模块负责 MPU6050 的非阻塞初始化、I²C 中断传输、原始数据读取和解析。模块不在函数内部轮询等待 I²C，因此应用主循环可以同时处理电机控制、串口命令和其他任务。

当前模块使用固定配置：

| 配置项 | 设置 |
| --- | --- |
| 采样率 | 100 Hz |
| `CONFIG` | `0x03`，启用数字低通滤波 |
| 陀螺仪量程 | ±250 dps，约 131 LSB/(°/s) |
| 加速度计量程 | ±2 g |
| 数据读取 | 从 `ACCEL_XOUT_H` 开始连续读取14字节 |

#### 数据与状态

一次读取会得到完整的三轴加速度、温度和三轴陀螺仪原始值：

```c
typedef struct {
    int16_t accelX;
    int16_t accelY;
    int16_t accelZ;
    int16_t temperature;
    int16_t gyroX;
    int16_t gyroY;
    int16_t gyroZ;
} MPU6050_RawData;
```

模块状态包括：

```c
typedef enum {
    MPU6050_STATUS_NOT_STARTED = 0,
    MPU6050_STATUS_INITIALIZING,
    MPU6050_STATUS_READY,
    MPU6050_STATUS_READING,
    MPU6050_STATUS_ERROR,
} MPU6050_Status;
```

| 状态 | 含义 |
| --- | --- |
| `NOT_STARTED` | 尚未调用 `MPU6050_Begin()` |
| `INITIALIZING` | 正在等待上电稳定、检查身份或写入配置 |
| `READY` | 初始化完成，并且当前可以启动一次读取 |
| `READING` | 一次14字节 I²C 读取正在进行 |
| `ERROR` | 参数、身份检查或初始化期间的 I²C 传输失败 |

#### 接口

```c
void MPU6050_Begin(
    I2C_Regs *i2c,
    uint8_t address,
    uint32_t nowMs);

void MPU6050_Process(uint32_t nowMs);
MPU6050_Status MPU6050_GetStatus(void);

bool MPU6050_StartRead(void);
bool MPU6050_GetData(MPU6050_RawData *data);

void MPU6050_HandleI2CInterrupt(void);

int32_t MPU6050_GyroRawToCentiDps(
    int32_t rawValue);
```

`MPU6050_Begin()` 开始初始化，但不会阻塞到初始化完成。模块内部会等待上电稳定、检查 `WHO_AM_I`、唤醒芯片并写入固定配置。

`MPU6050_Process()` 推进初始化和读取状态机，应用必须在主循环中持续调用，并传入单调递增的毫秒时间。

`MPU6050_StartRead()` 在模块处于 `READY` 状态时启动一次异步读取。模块正忙或上一份数据尚未取走时返回 `false`。

`MPU6050_GetData()` 在有一份新数据时复制到应用缓冲区并返回 `true`；同一份数据只会成功取出一次。

`MPU6050_GyroRawToCentiDps()` 按当前 ±250 dps 量程将原始量换算为 0.01 dps。静止零偏由应用先行扣除：

```c
int32_t correctedRaw;
int32_t gyroZCentiDps;

correctedRaw =
    (int32_t) sensorData.gyroZ - gyroZOffset;

gyroZCentiDps =
    MPU6050_GyroRawToCentiDps(correctedRaw);
```

#### SysConfig 要求

应用工程应在 SysConfig 中完成：

- 添加 I²C Controller 并配置总线速度。
- 分配 SDA 和 SCL 引脚。
- 启用 Controller `TX_DONE`、`RXFIFO_TRIGGER`、`RX_DONE`、`NACK` 和 `ARBITRATION_LOST` 中断。
- 确认 SDA、SCL 总线上具有合适的上拉电阻。

当前 `05_mpu6050` 使用 I2C0、400 kHz、PA0 SDA 和 PA1 SCL。

#### 基本用法

初始化前先执行 `SYSCFG_DL_init()`，然后启动模块并打开 CPU 侧 I²C 中断：

```c
SYSCFG_DL_init();

MPU6050_Begin(
    MPU6050_I2C_INST,
    MPU6050_ADDRESS_AD0_LOW,
    gMilliseconds);

NVIC_ClearPendingIRQ(MPU6050_I2C_INST_INT_IRQN);
NVIC_EnableIRQ(MPU6050_I2C_INST_INT_IRQN);
```

应用自己的中断入口只负责把处理交给模块：

```c
void MPU6050_I2C_INST_IRQHandler(void)
{
    MPU6050_HandleI2CInterrupt();
}
```

主循环持续推进模块，在需要的采样时刻调用 `MPU6050_StartRead()`：

```c
MPU6050_RawData sensorData;

while (1) {
    MPU6050_Process(gMilliseconds);

    if (MPU6050_GetData(&sensorData)) {
        /* 在这里校准、滤波或计算角度 */
    }

    if (MPU6050_GetStatus() ==
        MPU6050_STATUS_READY) {
        /* 到达应用设定的采样时刻后再启动 */
        MPU6050_StartRead();
    }

    __WFI();
}
```

模块需要毫秒时间只是为了处理上电和唤醒等待，不会自行决定连续采样周期。10 ms采样调度、静止零偏校准、滤波和角度积分仍由应用层负责。

当前第一版模块内部只保存一套设备状态，支持一个 MPU6050，并要求它独占传入的 I²C 控制器。若多个设备需要共享同一路 I²C，应在后续增加公共总线管理机制。

## 示例工程

### 01_LEDtest

`Examples/01_LEDtest` 是 `output` 模块当前的验证工程。SysConfig 保留了一个普通 GPIO，并配置了两路 PWM；当前交替呼吸程序实际操作的是两路 PWM。

#### 数字输出配置

PA14 保留为普通 GPIO，当前呼吸程序没有操作它；需要时可以使用：

```c
DigitalOutput_Toggle(USER_PORT, USER_LEDboard_PIN);
```

#### PWM 配置

| 配置项 | 设置 |
| --- | --- |
| PWM 实例名 | `LED_PWM` |
| 定时器 | `TIMG0` |
| 定时器时钟 | 32 MHz BUSCLK |
| 时钟分频与预分频 | 1、1 |
| PWM Period Count | 32000 |
| PWM 频率 | 约 1 kHz |
| PWM 模式 | Edge-aligned Down Counting |
| 更新方式 | Immediate |
| CCP0 输出 | PA12 |
| CCP1 输出 | PA13 |
| 输出反相 | 关闭 |
| Start Timer | 关闭，由应用代码启动 |

#### 交替呼吸效果

应用程序使用 SysTick 每 1 ms 更新占空比，完成以下 4 秒循环：

```text
0～1 s：PA12 由暗变亮，PA13 熄灭
1～2 s：PA12 由亮变暗，PA13 熄灭
2～3 s：PA12 熄灭，PA13 由暗变亮
3～4 s：PA12 熄灭，PA13 由亮变暗
```

两路呼吸 PWM 已在 MSPM0G3507 上烧录并实测成功。

#### 验证结果

该工程已经通过：

- SysConfig CLI 配置生成。
- ARM GCC 编译和链接。
- `-Wall -Wextra -Werror` 严格警告编译。
- `firmware.hex` 生成。
- PA12、PA13 双通道交替呼吸硬件实测。

### 02_hc05

`Examples/02_hc05` 是 `hc05` 模块的验证工程，使用以下配置：

| 配置项 | 设置 |
| --- | --- |
| UART 实例 | UART1 |
| 波特率 | 115200 |
| 数据格式 | 8-N-1 |
| MSPM0 TX | PA8 |
| MSPM0 RX | PA9 |
| 接收方式 | UART RX 中断和软件循环队列 |

上电后程序发送提示字符串。收到的数据先由中断放入模块循环队列，再由主循环逐字节取出并原样回传。

### 03_ADCread

`Examples/03_ADCread` 是 `adc` 模块的验证工程，同时复用 `hc05` 模块输出采样结果。

当前工程通过 PA27 读取 ADC0 通道0，每500 ms采样一次，并输出 ADC 原始值和按 3.3 V 参考电压换算的毫伏值。

| 配置项 | 设置 |
| --- | --- |
| ADC 实例 | ADC0 |
| 转换存储器 | ADCMEM0 |
| 输入通道 | Channel 0 |
| 模拟输入引脚 | PA27 |
| 采样时钟 | ULPCLK |
| 时钟分频 | 8 |
| 采样时间 | 125 μs |
| 电源模式 | Manual |
| 应用采样周期 | 500 ms |
| 调试输出 | HC-05，UART1，115200 |

#### 验证结果

该工程已经通过：

- SysConfig CLI 配置生成。
- ARM GCC 编译和链接。
- `-Wall -Wextra -Werror` 严格警告编译。
- `firmware.hex` 生成。
- 封装后的固件烧录实测，ADC 原始值读取、毫伏换算和 HC-05 输出正常。

### 05_mpu6050

`Examples/05_mpu6050` 是 `mpu6050` 模块的验证工程，同时复用 `hc05` 模块接收命令和输出测量结果。

| 配置项 | 设置 |
| --- | --- |
| I²C 实例 | I2C0 Controller |
| I²C 速度 | 400 kHz |
| SDA / SCL | PA0 / PA1 |
| MPU6050 地址 | `0x68`，AD0 接低电平 |
| 采样定时器 | TIMG0，每10 ms中断 |
| MPU6050 采样率 | 100 Hz |
| 陀螺仪量程 | ±250 dps |
| 加速度计量程 | ±2 g |
| 调试串口 | HC-05，UART1，115200 |

应用启动后等待 MPU6050 初始化完成，静止采集200份数据计算 Z 轴零偏，然后以100 Hz读取原始数据、计算 Z 轴角速度并积分得到累计转角。蓝牙命令保持为：

- `z`：累计角度归零。
- `r`：重新进行静止零偏校准。
- `p`：立即打印当前角速度和累计角度。

模块化后的职责划分为：

- `modules/mpu6050`：初始化、I²C 中断传输、原始数据解析和量程换算。
- `modules/hc05`：调试输出、UART RX 中断和接收队列。
- `Examples/05_mpu6050/mpu6050.c`：10 ms调度、200次校准、角度积分、命令和打印。

#### 验证结果

模块化后的工程已经通过：

- SysConfig CLI 配置生成。
- ARM GCC 编译和链接。
- `-Wall -Wextra -Werror` 严格警告编译。
- `firmware.hex` 生成。
- 模块化固件烧录实测，非阻塞初始化、连续采样、静止校准、角度积分、HC-05 输出及 `z`、`r`、`p` 命令均正常。

## MSPM0 平台与资源约束

### GPIO 与引脚复用

不是所有 GPIO 引脚都能输出指定定时器的 PWM。必须在 SysConfig 中确认：

- 引脚支持目标 TIMA/TIMG 的 CCP 功能。
- 引脚复用已经设置为 PWM。
- PWM 通道与代码中传入的 Capture/Compare 通道一致。

同一个引脚不能同时作为普通 GPIO 和定时器 PWM 输出。需要全灭或全亮时，可以把 PWM 占空比分别设置为 0% 或 100%，不必在运行中反复切换引脚复用。

### 定时器与多通道 PWM

同一个定时器的多个 PWM 通道共享：

- 时钟源。
- 分频和预分频。
- 计数器。
- 周期，也就是 PWM 频率。

各通道拥有独立的比较值，因此可以设置不同占空比。

在 MSPM0G3507 的常规独立 PWM 输出中：

- TIMG 和 TIMA1 通常提供 CC0、CC1 两个通道。
- TIMA0 最多提供 CC0～CC3 四个通道。
- 互补输出不是新的独立占空比通道。

最终可用通道数还受芯片封装、引脚复用和其他外设占用影响。

### 共用一个定时器与使用多个定时器

两个独立定时器可以通过相同的时钟和周期配置成相同频率，但它们的启动时刻不一定对齐，并且会占用更多定时器资源。

同一个定时器的多个通道天然同频并共用计数器，适合：

- 多个同频 LED 调光输出。
- 两个需要相同 PWM 频率的直流电机。
- 需要保持固定边沿关系的多通道输出。

如果不同输出需要不同频率，例如 LED 使用 1 kHz、电机使用 20 kHz，就应使用不同定时器。

### Start、Stop 与通道归属

`PwmOutput_Start()` 和 `PwmOutput_Stop()` 控制整个定时器，而不是单独的 PWM 通道。

多个通道共用同一个定时器时：

- `Start` 会启动所有已配置通道。
- `Stop` 会停止整个定时器。
- 修改某个通道的占空比只影响该通道。

如果只想关闭一个通道，应将该通道设置为 0%：

```c
PwmOutput_SetDuty(
    PWM_0_INST,
    DL_TIMERG_CAPTURE_COMPARE_0_INDEX,
    0U);
```

### 资源冲突

组合工程应统一规划 TIMA/TIMG，避免同一个定时器同时被不同模块配置为互不兼容的用途，例如：

- PWM 输出。
- 周期中断。
- 输入捕获。
- 编码器或 QEI。
- ADC 定时触发。

不要在公共模块中永久硬编码某个定时器实例。具体定时器和通道应由各应用工程的 SysConfig 分配，再通过生成宏传给模块。

### UART 接收队列

`hc05` 模块当前只有一个软件接收队列，因此同一程序中不要让两个 UART 实例同时调用 `HC05_HandleRxInterrupt()`。如果以后需要同时管理多个串口，应把接收队列改为由应用传入的设备结构体。

阻塞发送不会占用 UART RX 队列，但长时间连续发送会占用 CPU。接收速度持续高于主循环处理速度时，软件队列仍可能溢出，应通过 `HC05_RxOverflowed()` 检查。

### MPU6050 与 I²C 总线

`mpu6050` 模块内部保存 I²C 传输阶段、接收缓冲区和设备地址。当前版本只支持一个设备，并假设传入的 I²C 控制器不被其他模块同时操作。

应用必须在 SysConfig 和主函数中同时完成两层中断配置：

- SysConfig 启用 I²C 外设侧的相关中断源。
- 主函数通过 `NVIC_EnableIRQ()` 启用 CPU 侧中断。

`MPU6050_Process()` 只能放在主循环调用，不能放进 I²C ISR。ISR 只调用 `MPU6050_HandleI2CInterrupt()`。

## 常见问题

### 编译时找不到 `output.h`

检查 Makefile 是否加入：

```make
-I../../modules/output
```

同时确认工程目录和 `modules/` 的实际相对位置。

### 链接时找不到 `DigitalOutput_*` 或 `PwmOutput_*`

检查编译源文件列表是否加入：

```make
../../modules/output/output.c
```

只包含 `output.h` 不会自动编译 `output.c`。

### 数字输出没有反应

检查：

- 是否调用了 `SYSCFG_DL_init()`。
- SysConfig 是否将引脚配置为数字输出。
- 传入的端口宏和引脚宏是否匹配。
- 外部设备是否为低电平有效。

### PWM 没有波形

检查：

- SysConfig 是否添加并配置了 PWM 模块。
- PWM 输出引脚是否支持并选中了正确的 CCP 功能。
- 是否调用了 `PwmOutput_Start()`。
- TimerA/TimerG 通道宏是否与 SysConfig 一致。
- 定时器是否被其他模块停止或重新配置。

### PWM 亮暗关系相反

外部设备可能采用低电平有效，或者 SysConfig 开启了输出反相。检查 `Invert Channel` 配置，并确认应用代码中的占空比表示高电平时间还是设备有效时间。

### 修改一路 PWM 频率时另一路也发生变化

两路 PWM 很可能共用同一个定时器。频率由公共计数器周期决定，修改周期会影响该定时器的所有通道。

如果两路必须使用不同频率，应把它们分配到不同定时器。

### HC-05 能发送但收不到数据

检查：

- HC-05 TX 是否连接 MSPM0 RX，HC-05 RX 是否连接 MSPM0 TX。
- 两端是否共地。
- SysConfig 是否启用了 UART RX 中断。
- 主函数是否调用 `NVIC_EnableIRQ()`。
- UART 中断入口是否调用 `HC05_HandleRxInterrupt()`。

### HC-05 接收数据丢失

调用 `HC05_RxOverflowed()` 检查循环队列是否溢出。若返回 `true`，应让主循环更及时地读取数据、降低发送端速率，或在后续版本中扩大接收队列。

### MPU6050 一直停在初始化或进入错误状态

检查：

- AD0 电平是否与传入的 `0x68` 或 `0x69` 地址一致。
- SDA、SCL 是否接反并具有上拉电阻。
- SysConfig 是否配置为 I²C Controller。
- I²C 外设中断源和 NVIC 中断是否都已启用。
- I²C IRQHandler 是否调用 `MPU6050_HandleI2CInterrupt()`。
- 主循环是否持续调用 `MPU6050_Process()`，传入的毫秒时间是否持续递增。

### MPU6050 初始化完成但没有新数据

检查应用是否在设定的采样时刻调用 `MPU6050_StartRead()`，并通过 `MPU6050_GetData()`及时取走上一份数据。模块正忙或上一份新数据尚未取走时，`MPU6050_StartRead()`会返回 `false`。

## 协作与扩展约定

### 使用公共模块

- 公共实现统一放在 `modules/`，不要复制进各个示例工程。
- 新工程通过 Makefile 直接引用公共模块源文件。
- 每个工程保留自己的 SysConfig 硬件资源配置。
- 应用代码优先使用 SysConfig 生成宏，不硬编码端口、引脚和通道。

### 修改公共接口

- 修改 `modules/` 中的公开接口时，同时更新本说明文档。
- 修改后至少选择一个相关示例工程完成编译验证。
- 涉及硬件行为的修改应在开发板上实测。
- 不要直接修改 SysConfig 自动生成文件。

### 添加新模块

新增模块时建议保持以下结构：

```text
modules/
└── module_name/
    ├── module_name.c
    └── module_name.h
```

同时在本文档中补充：

- 模块定位。
- 公共 API。
- SysConfig 前置配置。
- 最小使用示例。
- 平台与资源限制。
- 已验证的示例工程。
