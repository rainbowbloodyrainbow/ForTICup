# Codex 日志

## 日志约定

- **目的和作用**：保存用户认为值得跨对话保留的工作区背景、已完成事项和关键结论，供未来的 Codex 快速恢复上下文。
- **写日志要求**：内容应简短，只记录未来对话无法直接得知的信息；避免复述通用知识、详细教程和完整对话；每条日志的时间应精确到小时。
- **写入时机**：平时不得擅自记录或更新日志。只有用户明确认为当前对话内容值得记录，或明确要求写日志时，才可写入。

## 当前学习背景（更新于 2026-07-17 22:00，Asia/Shanghai）

- 用户计划使用 LED、旋转电位器、HC-05、SSD1306、旋转编码器和串口屏等简单器件，组合模拟并练习电赛中可能遇到的应用场景。

## 2026-07-17 13:00（Asia/Shanghai）

- 用户正在用 MSPM0G3507 准备全国大学生电子设计竞赛，目前从 `01_LEDtest` 学起。
- 工作区外安装有 TI SDK：`/home/clbritheking/ti/mspm0_sdk_2_11_00_07`。该目录可读；遇到 DriverLib 问题时，优先检查其中的实际头文件和示例。
- 当前工程使用 MSPM0 SDK 2.11.00.07、SysConfig 和 32 MHz CPU 时钟。
- 已结合本机 SDK 讲解 GPIO DriverLib、`DL_Common_delayCycles()` 和 SysTick 非阻塞计时。
- 已指出当前 `main.c` 中 `DL_Common_delayCycles(64000000)` 在 32 MHz 下约为 2 秒，不是注释中的 0.5 秒。
- 已解释 CMSIS、TI DriverLib、FreeRTOS 和 CMSIS-RTOS2 的关系；特别说明 CMSIS-RTOS2 是统一 API，不是 FreeRTOS 的衍生系统。
- 给出的电赛建议：先掌握裸机事件驱动、状态机、中断、定时器和 DMA；同时学习 FreeRTOS 作为复杂项目的备用方案。若在当前 MSPM0G3507 上使用 RTOS，优先选择 SDK 自带的 FreeRTOS 原生 API，不优先考虑 Zephyr 或 CMSIS-RTOS2 适配层。
- 本机可参考 FreeRTOS 示例：`/home/clbritheking/ti/mspm0_sdk_2_11_00_07/examples/rtos/LP_MSPM0G3507/`。

## 2026-07-17 18:00（Asia/Shanghai）

- 继续讨论了用 SysTick 毫秒计数替换 `DL_Common_delayCycles()`，并讲解了中断共享变量使用 `volatile` 的原因。
- 已明确：SysTick 是 Cortex-M0+ 内核硬件定时器，`SysTick_Handler()` 是其中断处理函数，不是软件模拟定时器。
- 经本机 SDK 核对，MSPM0 SDK 2.11.00.07 的 SysConfig 实际提供 `SYSTICK` 模块；之前“无需通过 SysConfig 配置”的说法不完整。
- 当前工程推荐通过 SysConfig 设置 SysTick 周期为 32000 MCLK cycles、开启中断并启动计数；`main.c` 只保留毫秒计数、中断函数和非阻塞业务逻辑。不要直接修改生成的 `ti_msp_dl_config.c/.h`。

## 2026-07-19 06:00（Asia/Shanghai）

- 已将示例入口文件按学习模块重命名：`01_LEDtest/led.c`、`02_ADCread/ADC.c`，并同步修正各工程 Makefile；用户暂时不做模块函数封装。
- `02_ADCread` 使用 PA27（ADC0 通道 0、ADCMEM0）轮询采集 12 位 ADC，程序保留原始值和按 3.3 V 参考换算的毫伏值供调试器观察。
- 已建立 `03_hc05`：HC-05 使用 UART1、115200、8-N-1，PA8 为 MSPM0 TX、PA9 为 MSPM0 RX，并启用 RX 中断。最初无法通信的实际原因是 HC-05 的 RX/TX 接反，修正后已成功。
- `03_hc05/hc05.c` 当前恢复为纯回显测试：上电发送提示，收到电脑/蓝牙端字节后立即原样回传；此前 A/B 控制板载灯的实验代码及 GPIO 配置已移除。
- 当前 Makefile 只生成 `firmware.elf/.hex`，不会自动烧录，测试新代码时仍需另行下载到 MSPM0。
- 已结合本机 SDK 说明 UART 直接、阻塞、带状态检查、FIFO 和中断式收发 API；`HC05_UART_INST` 是 SysConfig 名称 `HC05_UART` 对 UART1 实例生成的宏别名，引脚复用由独立 GPIO/IOMUX 配置负责。
- 后续计划尝试自定义字符串发送/接收封装：发送可遍历至 `\0`；接收必须同时规定缓冲区容量和结束条件。当前 RX 中断会先取走硬件数据，不能再与阻塞接收同时使用；较可靠的方向是 RX 中断写入循环队列、主循环组帧和解析字符串。

## 2026-07-19 07:00（Asia/Shanghai）

- 为符合学习依赖顺序，工程编号已调整为 `02_hc05` 和 `03_ADCread`，相应目录、SysConfig 文件名及 Makefile 引用均已同步修改。
- `03_ADCread` 已合并 HC-05 串口输出：保留 PA27 的 ADC0 通道 0采集，同时加入 UART1（115200、PA8 TX、PA9 RX），当前不启用 RX 中断。
- ADC 程序使用 SysTick 每 500 ms 采样并通过蓝牙串口输出 `ADC=<原始值>, Voltage=<毫伏值> mV`；新增阻塞式字符串和无符号十进制整数发送辅助函数，不依赖 `printf`。
- 合并后的 `03_ADCread.syscfg` 已通过 SysConfig CLI 校验，工程已成功生成并编译。
- ADC 最初在 PA27 上始终读到 0；改用 PA26 并补充稳妥采样配置后恢复正常，再仅切回 PA27 复测也能覆盖 0～4095，证明 PA27 未损坏，问题来自原先过度依赖默认值的 ADC 配置。
- `03_ADCread` 最终保留 PA27（ADC0 channel 0），使用 ULPCLK、8 分频、125 μs 采样时间和手动电源模式；该组合已通过电位器实测。

## 2026-07-19 15:00（Asia/Shanghai）

- 已建立 `04_ssd1306`；当前 SysConfig 使用 I2C0 Controller、100 kHz、PA0 SDA 和 PA1 SCL，未开启 Target、10 位寻址、中断和 DMA。配置已通过 SysConfig CLI 校验，生成的引脚复用正确。
- SSD1306 使用 7 位 I²C 地址，常见为 `0x3C`；Pin Configuration 的额外开关暂时无需启用，但需确认 SDA/SCL 具有上拉电阻。
- 当前先采用 I²C 轮询发送：“轮询”指 CPU 在一次传输内查询 I²C 状态直至完成，不是在主循环中无条件重复刷屏；I²C 中断可在日后用于补充 TX FIFO、处理 TX_DONE/NACK，但初次点屏不需要。
- 用户确定按以下顺序自行实现，暂不使用现成 SSD1306 库：发送单个命令 → 最小初始化 → 全显存写 `0xFF` 点亮 → 写 `0x00` 清屏 → 理解 Page/Column 并画像素或方块 → 实现直线、字符和数字。

## 2026-07-23 21:00（Asia/Shanghai）

- `04_ssd1306/ssd1306.c` 已实现阻塞轮询式 `OLED_SendCommand()`：发送 `0x00 + command` 两字节，等待 IDLE/BUSY，检查 ERROR 和 ARBITRATION_LOST，并按 TI 勘误在启动传输后延时 3 个 I²C 功能时钟周期。
- 主函数已逐条发送 128×64 SSD1306 初始化命令，开启内部电荷泵和显示，最后用 `0xA5` 忽略 GDDRAM 并强制全屏点亮；用户已烧录并实测成功。
- `04_ssd1306/Makefile` 已由前一工程调整，使用 `04_ssd1306.syscfg` 和 `ssd1306.c`，可正常生成固件。
- 当前只是通过 `0xA5` 验证整屏发光，还未真正向 1024 字节 GDDRAM 写入 `0xFF`；下一步应回到 `0xA4` 正常显存显示模式，再实现数据发送。
- 用户当前更易理解显式展开的 C 写法，如 `while(1)` + 状态变量 + `if` + `break`；后续教学和代码应优先沿用该风格，避免过早压缩逻辑或引入过多抽象。

## 2026-07-24 23:00（Asia/Shanghai）

- 用户暂时搁置 `04_ssd1306` 的显存与绘图完善，先学习 `05_mpu6050`。
- `05_mpu6050` 使用 I2C0、400 kHz、PA0 SDA、PA1 SCL；AD0按低电平地址 `0x68` 使用，XDA/XCL和MPU6050 INT暂不连接。采样由 TIMG0 每10 ms触发，I²C采用FIFO中断状态机完成寄存器写和重复起始连续读，运行时不阻塞等待I²C；DMA留待后续升级。
- 工程已合入HC-05：UART1、115200、PA8 TX、PA9 RX及RX中断。`mpu6050.c` 会异步检查 `WHO_AM_I=0x68`，配置MPU6050为100 Hz、陀螺仪±250 dps、加速度计±2 g，静止采集200次校准Z轴零偏，再积分并每100 ms输出Z轴角速度和累计转角；蓝牙命令 `z`、`r`、`p` 分别用于角度归零、重新校准和立即打印。
- `05_mpu6050.syscfg` 已通过本机SysConfig CLI校验，工程已用GCC完整编译并生成 `firmware.elf/.hex`；用户已经烧录并实测通信、初始化、校准和连续输出正常。
- 本次实测初始Z轴零偏为 `-202 LSB`（约 `-1.54°/s`）；热启动静止10分钟后累计角度约 `-8.20°`，对应剩余漂移约 `-0.82°/min`、`-0.0137°/s`或 `-1.8 LSB`。该表现适合秒级小车转向，但不适合单独维持长期绝对航向。
- 已明确：MPU6050六轴数据可用加速度计纠正横滚/俯仰，但无法为Z轴航向提供绝对参考；卡尔曼滤波若要真正约束小车航向漂移，需要编码器、磁力计、视觉等独立观测，其中差速轮编码器是后续优先考虑的融合来源。

## 2026-07-25 06:00（Asia/Shanghai）

- 用户决定重构现有 `01`～`05` 工程，将 LED、HC-05、MPU6050 等分别做成可复用模块；组合工程应直接调用这些模块目录中的实现，而不是在每个示例目录复制一套驱动代码。
- 用户重新评估了 `04_ssd1306` 的学习优先级：MPU6050同样可以承担I²C学习任务，日常调试优先复用HC-05串口，因此SSD1306暂不作为当前重点。
- LED模块的核心定位是GPIO电平输出，后续按普通高低电平和PWM输出两类能力设计。
- 已确认MSPM0G3507只有一路低电流模拟DAC输出，不能提供四路DAC，也不能直接连接或驱动两个电机；电机两端应连接H桥输出，单片机只控制驱动器的逻辑输入。
- 两个直流电机通常可共用一个定时器的两个PWM通道，并配合方向GPIO，不需要为四个驱动输入分别占用定时器。用户据此决定暂时不学习DAC，优先掌握GPIO、PWM和电机驱动。

## 2026-07-25 14:00（Asia/Shanghai）

- 已建立通用输出模块 `modules/output/output.c/.h`，不再局限于 LED；提供 `DigitalOutput_High/Low/Write/Toggle()` 和 `PwmOutput_Start/Stop/SetDuty()`。
- 数字输出接口统一传入 `GPIO_Regs *port` 和 `pin`；PWM 占空比采用 `0～1000` 千分比，当前换算面向 SysConfig 配置的边沿对齐、非反相 PWM，具体定时器、通道和引脚仍由各组合工程分配。
- `01_LEDtest` 已通过 Makefile 直接引用公共 `output` 模块，原有 LED1/LED2 翻转已改用 `DigitalOutput_Toggle()`；SysConfig 生成、GCC 编译和 HEX 生成均已通过。该工程尚未配置并实测 PWM。
- 根目录新增 `myownlib使用说明.md`，记录公共库的 Makefile 接入方法、数字输出与 PWM API、示例、资源约束和常见问题，供队友共享使用。

## 2026-07-25 22:00（Asia/Shanghai）

- 示例工程已统一移入根目录的 `Examples/`；`Examples/01_LEDtest/Makefile` 和 `myownlib使用说明.md` 中引用公共 `modules/output` 的相对路径已同步调整。
- `Examples/01_LEDtest` 当前只保留 PA14 为普通 GPIO；新增 `LED_PWM`，使用 TIMG0、32 MHz BUSCLK、周期 32000（约 1 kHz），CCP0 输出到 PA12、CCP1 输出到 PA13。
- `led.c` 已实现 4 秒循环的双路交替线性呼吸：PA12 在 0～2 秒完成亮灭，PA13 在 2～4 秒完成亮灭；两路每 1 ms 更新占空比，共用并只启动一次 TIMG0。
- 新配置已通过 SysConfig CLI、GCC 完整构建及 `-Wall -Wextra -Werror` 严格编译，用户已烧录并确认 PA12、PA13 交替呼吸实测成功；`PwmOutput` 因此完成硬件验证。

## 2026-07-25 23:00（Asia/Shanghai）

- `myownlib使用说明.md` 已由数字编号的线性章节重组为“项目概览、快速接入、模块说明、示例工程、平台资源约束、常见问题、协作与扩展约定”的语义层级；`DigitalOutput` 和 `PwmOutput` 均归入 `output` 模块。
- 已交叉核对 `modules/output`、`Examples/01_LEDtest` 的 SysConfig/代码/Makefile、使用说明和日志；修正了头文件对 PWM 模式的表述，明确 `PwmOutput_SetDuty()` 当前面向边沿对齐向下计数、非反相 PWM。
- 使用说明已明确 PA14 只是保留为普通 GPIO，当前交替呼吸程序实际操作 PA12/PA13 两路 PWM；一致性检查后再次通过 SysConfig CLI 和 `-Wall -Wextra -Werror` 完整构建。

## 2026-07-27 23:00（Asia/Shanghai）

- 已建立公共 `modules/hc05/hc05.c/.h`：提供阻塞式字节、缓冲区、字符串、十进制整数和十六进制发送，以及 UART RX 中断接收接口。
- 接收使用模块内部的 64 字节循环数组，实际最多暂存 63 字节；UART 中断负责入队，主循环通过 `HC05_ReadByte()` 取出，队列满时丢弃新字节并设置溢出标志。当前接收状态只支持一路 UART。
- `Examples/02_hc05` 已直接引用公共模块并改为“中断入队、主循环回显”；SysConfig CLI、普通构建及 `-Wall -Wextra -Werror` 严格编译均通过，重构后的固件尚待开发板复测。
- 当前模块已足以承担电赛常见的调试输出、状态量上报和短命令接收；115200 波特率下每 100 ms 输出几十到一百多字节通常可行。发送仍为阻塞式，大数据量或高实时性场景再考虑 TX 中断队列或 DMA。
- `myownlib使用说明.md` 已补充 HC-05 的接入、API、SysConfig 要求、中断流程、循环队列限制、常见问题和 `02_hc05` 示例说明。

## 2026-07-28 00:00（Asia/Shanghai）

- 已建立公共 `modules/adc/adc.c/.h`，提供阻塞读取 `ADCMEM0` 的 `ADC_ReadRaw()` 和12位原始值转毫伏的 `ADC_RawToMillivolts()`；采样调度、滤波、标定和阈值判断留给上层。
- `Examples/03_ADCread` 已直接引用公共 `adc` 与 `hc05` 模块；SysConfig、普通及严格构建均通过，用户已烧录确认 ADC 读取、毫伏换算和蓝牙输出正常。
- 已建立公共 `modules/mpu6050/mpu6050.c/.h`：提供非阻塞初始化、状态查询、14字节异步读取、六轴原始数据解析、I²C中断处理和当前 ±250 dps 量程换算。第一版内部只保存一套状态，支持一个 MPU6050 独占一路 I²C控制器。
- `Examples/05_mpu6050` 已改为直接引用公共 `mpu6050` 与 `hc05` 模块；10 ms调度、200次Z轴零偏校准、角度积分、漏采样统计和 `z/r/p` 命令仍属于应用层。工程通过 SysConfig、普通及 `-Wall -Wextra -Werror` 构建，用户烧录确认初始化、连续采样、校准、积分、输出和命令均正常。
- `myownlib使用说明.md` 已补充 ADC、MPU6050 的接入方法、接口、状态机、SysConfig要求、示例、资源限制、常见问题和硬件验证结果。
