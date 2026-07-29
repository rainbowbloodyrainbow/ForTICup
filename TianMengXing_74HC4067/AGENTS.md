# AGENTS.md — 天猛星 MSPM0G3507 工程速查

写给在本项目里干活的 AI / 开发者：以下是已查证和已实测的事实，直接用，不要重复踩坑。

## 开发环境（Windows 本机路径）

- MSPM0 SDK：`D:/embedded_tools/ti/mspm0_sdk_2_10_00_04/source`
- SysConfig：`D:\embedded_tools\ti\sysconfig_1.28.0`
- OpenOCD：`D:\embedded_tools\ti\openocd_1.5.0.75\openocd\bin\openocd.exe`
- 烧录器：DAPLink / CMSIS-DAP（连接后弹出的 `DAPLINK (E:)` U 盘窗口无需任何操作，直接关掉）

## 构建与烧录

```bash
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel
```

- 注意：`CMakeLists.txt` 用 `file(GLOB_RECURSE)` 收集 `src/*.c`，**新增源文件后必须重新跑 configure**（第一条命令），否则新文件不参与编译。
- 烧录命令（即 VS Code 任务 `Flash MSPM0`）：

```bash
D:\embedded_tools\ti\openocd_1.5.0.75\openocd\bin\openocd.exe \
  -s D:\embedded_tools\ti\openocd_1.5.0.75\openocd\share\openocd\scripts \
  -f interface/cmsis-dap.cfg -f target/ti_mspm0.cfg \
  -c "adapter speed 2000" \
  -c "program build/tianmengxing_mspm0g3507.elf reset exit"
```

- `config/ti_msp_dl_config.c/h` 由 SysConfig 自动生成，**不要手改**；目前 SysConfig 只配置了基础时钟和 GPIOA/GPIOB 电源，外设引脚目前都是在 `src/main.c` 里用 driverlib 直接配置的。

## 硬件事实（已实测 ✅ / 待验证 ⚠️）

- ✅ 板载 LED1 = **PB22**（`IOMUX_PINCM50`），**高电平点亮**（低电平会灭，实测确认）
- ⚠️ 板载按键 KEY = **PB21**（来自立创官方教程，未实测）
- ✅ 时钟：SYSOSC 32 MHz，HFXT / SYSPLL 均未启用
- ✅ GPIOA / GPIOB 电源已在 `SYSCFG_DL_GPIO_init()` 中打开，直接用 driverlib 配引脚即可
- 开发语言：全程使用 TI DriverLib（DL 库），工程链接 SDK 预编译的 `driverlib.a`，不要直接操作寄存器

## 舵机 / PWM（TIMG7）查证结果

目标：50 Hz PWM（周期 20 ms），1.5 ms 脉宽 = 180° 舵机中位。

- 32 MHz 总线时钟 → 预分频 16 → 2 MHz 计数频率 → 周期 40000 计数 = 20 ms → 中位比较值 3000（1.5 ms）。TIMG7 为 16 位定时器，40000 不溢出。
- TIMG7_CCP0 可用引脚（PINCM 索引来自 SDK 例程和 SysConfig 器件数据）：
  - PB15 = `IOMUX_PINCM32`（SDK 例程验证 ✅）
  - PA17 = `IOMUX_PINCM39`（SDK 例程验证 ✅）
  - PA26 = `IOMUX_PINCM59`（器件数据确认 PINCM 索引 ✅；有天猛星项目用 PA26/PA27 做 TIMG7 CH0/CH1，PF 功能号待烧录验证 ⚠️）
- 接线提醒：舵机电源接板子 5V 和 GND（**不要**从 MCU 引脚取电），信号线接 PWM 引脚，必须共地。

## 关键查询入口（下次查引脚直接来这里）

- SysConfig 器件数据（引脚 ↔ PINCM ↔ 封装映射，JSON）：
  `D:/embedded_tools/ti/sysconfig_1.28.0/dist/deviceData/MSPM0G350X/MSPM0G350X.json`
  （`devicePins` 里有每个引脚的 `iomux_pincm` 属性）
- TI 官方例程（含 SysConfig 生成的 `ti_msp_dl_config.h`，注释里写明引脚/PINCM/封装脚位）：
  `D:/embedded_tools/ti/mspm0_sdk_2_10_00_04/examples/nortos/LP_MSPM0G3507/`

## 参考资料链接

- 立创官方教程（天猛星）：https://wiki.lckfb.com/zh-hans/tmx-mspm0g3507/keil-beginner/
  - 点亮 LED 灯（PB22）、外部中断（PB21 按键翻转 PB22 LED）等章节
- 全国电赛培训网 · 天猛星资源汇总：https://www.nuedc-training.com.cn/index/huodong/mspm0_card13
- 开发板硬件开源（原理图）：https://oshwhub.com/li-chuang-kai-fa-ban
