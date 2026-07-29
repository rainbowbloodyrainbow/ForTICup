# 天猛星 MSPM0G3507 VS Code 工程

本工程面向立创·天猛星 `MSPM0G3507`（`LQFP-64(PM)`，完整 80P 接口），由
TI Project Assistant 生成，使用 CMake、Ninja 和 Arm GNU Toolchain。

## 开始写代码

- 应用入口：`src/main.c`
- 头文件：`inc/`
- 驱动实现：`src/driver/`
- 功能模块：`src/modules/`
- 应用层：`src/app/`
- SysConfig 配置：`tianmengxing_mspm0g3507.syscfg`
- 硬件实测笔记与环境速查：`AGENTS.md`（AI 工具会自动读取）

不要手工修改 `config/ti_msp_dl_config.c` 和
`config/ti_msp_dl_config.h`，它们由 SysConfig 自动生成。

## VS Code 操作

1. 用 VS Code 打开本文件夹。
2. 按 `Ctrl+Shift+B` 构建。
3. 运行任务 `⚙️ 打开 TI SysConfig` 配置时钟、引脚和外设。
4. 保存 `.syscfg` 后，在终端运行 `mspm0-init regenerate`。
5. 使用 DAPLink/CMSIS-DAP 连接板子的 SWD 接口后，运行任务
   `Flash MSPM0` 烧录，或按 `F5` 调试。

板载 Type-C 串口的一线 BSL 烧录与 CMSIS-DAP 调试是两种不同方式。
当前 VS Code 的烧录和调试任务按 DAPLink/CMSIS-DAP 配置。

## 命令行

```powershell
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel
mspm0-init regenerate
```

构建产物位于 `build/`，包括 `.elf`、`.hex`、`.bin` 和 `.map`。

## 当前实测功能

`src/main.c` 是四轮前轮转向小车的静态循迹测试固件：

- 后轮电机控制脚保持低电平，测试期间不会驱动车轮。
- 舵机信号为 `PA22 / TIMG6_CCP1`，机械中位为 `2850`。
- 开机仅在中位附近小范围摆动，然后在白底自动标定八路传感器。
- 黑线位置采用八路相对强度加权计算，并控制前轮舵机修正方向。
- `B21` 短接 GND 可重新执行白底标定。
- `UART0 TX PA10` 以 `115200 8N1` 输出八路 ADC、位置和舵机计数。

### 74HC4067 接线

| 74HC4067 | MSPM0G3507 |
| --- | --- |
| `S0` | `PB15` |
| `S1` | `PB16` |
| `S2` | `PB12` |
| `S3` | `PB13` |
| `SIG/COM` | `PB25 / ADC0.4` |
| `EN` | `GND` |
| `VCC` | `3.3V` |
| `GND` | `GND` |

八路模拟输出从左到右接入 `C0`～`C7`。上电校准时必须让八路探头全部位于白色背景上。
