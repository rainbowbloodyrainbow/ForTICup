# 地猛星 MS42CG + D36A 位置—速度双环闭环

上电时的电机位置定义为 `0°`。从电机输出轴端正对电机观察：

- 逆时针为正角度；
- 顺时针为负角度；
- `To30` 转到相对零点 `+30°`；
- `To-30` 转到相对零点 `-30°`；
- `To0` 返回上电零点。

到达目标后进入 `HOLDING` 状态并持续读取编码器。例如执行 `To12` 后，用手把电机轴拧离12°，控制器会重新发出STEP脉冲，将电机拉回12°。只有发送 `S` 才会退出闭环并关闭驱动使能。

这里控制的是**电机轴角度**，不是摆杆角度。电机轴、丝杠位移和摆杆角度之间还需要单独做机械标定。

## 视觉PID与电机双环结构

OpenMV给出球的横坐标后，最外层球位PID先计算电机目标角度；电机位置—速度双环只负责准确跟随这个动态角度：

```text
OpenMV横坐标
   ↓
球位PID：像素误差 → 对称倾斜需求u
   ↓
非对称机械映射：u → 电机目标角度
   ↓
电机位置P外环：角度/计数误差 → 目标编码器速度
   ↓
电机速度PI内环：速度误差 → STEP脉冲频率
   ↓
D36A + MS42CG + 编码器反馈
```

球位PID以 `x=180` 为目标，PID输出不是电机角度，而是对称的倾斜需求 `u`。代码用千分量表示：`u_milli=+1000/-1000` 代表大小相同、方向相反的一组参考倾斜需求。

```text
error = 180 - x
u = Kp×error + Ki×∫error·dt + Kd×d(error)/dt

u = 0      → motor_angle = 0°
u = +1000  → motor_angle = +30°
u = -1000  → motor_angle = -40°
```

正负方向分别映射，从而补偿丝杠机构两侧角度—倾斜度不对称。PID倾斜需求限制为±1500，因此电机视觉目标硬限额为 `+45°/-60°`。

控制器每10ms读取一次编码器，速度按下式计算并做一阶低通滤波：

```text
编码器速度 = (本次位置计数 - 上次位置计数) × 1000 / 实际间隔ms
```

没有在电机位置环加入I和D：速度内环已经提供阻尼和稳态修正，同时可以避免编码器毛刺被位置D项放大。当前STEP频率限制为1000 pulse/s，和此前1kHz固定脉冲的最高速度一致，但现在会随误差自动减速。

初始调参宏集中在 `main.c` 顶部：

```c
POSITION_KP_NUM / POSITION_KP_DEN   // 位置环P
SPEED_KP_NUM / SPEED_KP_DEN         // 速度环P
SPEED_KI_DIVISOR                    // 速度环I；数值越小，积分越强
MAX_TARGET_SPEED_COUNTS_S           // 目标编码器速度上限
MAX_STEP_RATE_PPS                   // STEP频率安全上限
VISION_KP/KI/KD_U_MILLI...          // 球位PID参数
VISION_POS_REF_MILLIDEG             // u=+1000对应+30°
VISION_NEG_REF_MAG_MILLIDEG         // u=-1000对应-40°
VISION_PID_U_LIMIT_MILLI            // ±1500，对应+45°/-60°
```

## 接线

| 信号 | 地猛星 | 功能 |
|---|---|---|
| 编码器 A | PA1 / PINCM2 | TIMG8_CCP0 / PHA |
| 编码器 B | PA0 / PINCM1 | TIMG8_CCP1 / PHB |
| D36A ST1/STEP | PB2 / PINCM15 | STEP脉冲 |
| D36A DIR1 | PB3 / PINCM16 | 方向 |
| D36A EN1 | PB6 / PINCM23 | 高电平使能 |
| 调试串口 TX | PA10 / PINCM21 | UART0_TX |
| 命令串口 RX | PA11 / PINCM22 | UART0_RX |
| OpenMV串口 TX | PA8 / PINCM19 | UART1_TX，可选接OpenMV P5 |
| OpenMV串口 RX | PA9 / PINCM20 | UART1_RX，接OpenMV P4 |

编码器、地猛星和D36A控制接口必须共地。编码器VCC按产品标签接3.3V或5V，不要只根据线色判断。

## SysConfig

`qei_test.syscfg` 已配置：

1. `QEI / ENCODER_QEI`：
   - 外设 `TIMG8`；
   - 2输入模式，不使用IDX；
   - PHA/CCP0为PA1；
   - PHB/CCP1为PA0；
   - Load Value为65535。
2. `GPIO / MOTOR_GPIO`：
   - STEP为PB2，初始低；
   - DIR为PB3，初始低；
   - EN为PB6，初始低，防止上电直接使能。
3. `UART / DEBUG_UART`：
   - UART0，115200、8-N-1；
   - TX/RX为PA10/PA11；
   - 开启FIFO。
4. `UART / OPENMV_UART`：
   - UART1，115200、8-N-1；
   - TX/RX为PA8/PA9；
   - 开启FIFO。

## OpenMV联调

OpenMV H7 Plus使用UART3：P4为TX、P5为RX。最小接线为：

```text
OpenMV P4 / UART3_TX  -> 地猛星 PA9 / UART1_RX
OpenMV GND            -> 地猛星 GND
```

如果需要地猛星向OpenMV回传，再接：

```text
地猛星 PA8 / UART1_TX -> OpenMV P5 / UART3_RX
```

烧入/运行 `openmv_ball_x.py`。OpenMV发送纯ASCII行协议：

```text
X123\r\n    检测到球，横坐标为123
N\r\n       当前帧未检测到球
```

地猛星对每一个有效坐标执行球位PID：

```text
error = 180 - x
x < 180  -> PID倾斜需求u为正，按+30侧比例映射
x > 180  -> PID倾斜需求u为负，按-40侧比例映射
x≈180    -> u趋近0，电机目标趋近0°
视觉角度限幅 -> -60°～+45°
无球     -> 保持上一个有效目标，并重置PID时间基准
```

每个有效坐标都会更新目标角度，但不会重置电机速度环。像素中心带有±2像素死区，D项带一阶低通滤波。每10个有效坐标输出一条 `VISION_PID`，包含像素误差、P/I/D分量、目标角度、当前角度和STEP频率。

## 命令

所有命令后都要发送回车或换行：

```text
To30       转到+30°
To-30      转到-30°
To12.5     转到+12.5°
To0        返回上电零点
V          开启OpenMV视觉控制
?          查询位置、速度、目标速度、STEP频率和状态
S          停止、使D36A失能并关闭视觉控制
```

固件上电后默认开启视觉控制，但在收到第一个有效 `X...` 坐标前电机保持失能，不会自动执行固定角度往返。手动 `To...` 或 `S` 会关闭视觉控制；发送 `V` 后重新进入视觉PID。

视觉联调日志示例：

```text
VISION_PID x=150 err_px=30 p_u_milli=300 i_u_milli=... d_u_milli=... u_milli=... target_deg=... current_deg=... step_pps=...
```

安全起见，当前命令范围限制为 `-90°～+90°`。机械行程确认后再修改 `MAX_ABS_TARGET_MILLIDEG`。

## 第一次上电必须确认的参数

### 1. 编码器方向

D36A保持失能时，手动逆时针转动输出轴，然后发送 `?`。如果 `angle_deg` 为负，修改 `main.c`：

```c
#define QEI_TO_CCW_POSITIVE_SIGN (-1LL)
```

### 2. DIR方向

先发送小角度命令：

```text
To5
```

若程序报告：

```text
FAULT feedback direction reversed
```

修改：

```c
#define DIR_LEVEL_FOR_POSITIVE_ANGLE (0U)
```

不要直接用大角度验证方向。

### 3. 每圈计数

旧MS42CG工程写 `1000×4=4000`，交接记录写 `1024×4=4096`。当前暂按4000：

```c
#define ENCODER_COUNTS_PER_REV (4000LL)
```

请准确手转一圈并读取 `count`。如果变化量为4096，则改成4096，否则角度会有约2.4%的比例误差。

## 保护行为

- 64个STEP脉冲后编码器变化仍小于4计数：停止并使驱动失能；
- 编码器反馈方向与命令方向相反：停止并使驱动失能；
- 速度PI积分带限幅，STEP输出限制在1000 pulse/s；
- 到位后停止STEP、保持EN为高并持续监测编码器；受到外力偏离目标超过容差后自动纠偏；
- `S`命令会把EN拉低，电机不再保持。

## 构建

```bash
cd /mnt/32FA76A4FA7663CF/study/ForTICup/ClBrIsWorks/Ball_and_Beam_stepin
make
```

烧录文件：

```text
build/qei_test.hex
```

根据现有串口情况，使用COM14发送命令、COM18读取日志。
