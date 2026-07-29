# H-BALL ESP32-S3 OV2640 独立无线图传

本工程使用纯 ESP-IDF，在 ESP32-S3 上建立独立 SoftAP，并通过 ESP-IDF 自带
`esp_http_server` 提供单帧 JPEG、状态 JSON 和单客户端 MJPEG 视频流。不依赖
Arduino、外部路由器、CDN 或第三方 Web 服务器。

## 硬件规格

- ESP32-S3 QFN56 rev 0.2
- 16 MB、3.3 V Quad Flash（QIO，80 MHz）
- 8 MB、3.3 V Octal PSRAM（80 MHz）
- 40 MHz 晶振
- OV2640 DVP 摄像头，XCLK 20 MHz

| OV2640 信号 | ESP32-S3 GPIO |
|---|---:|
| PWDN | 未连接（-1） |
| RESET | 未连接（-1） |
| XCLK | 15 |
| SIOD / SCCB SDA | 4 |
| SIOC / SCCB SCL | 5 |
| Y9 / D7 | 16 |
| Y8 / D6 | 17 |
| Y7 / D5 | 18 |
| Y6 / D4 | 12 |
| Y5 / D3 | 10 |
| Y4 / D2 | 8 |
| Y3 / D1 | 9 |
| Y2 / D0 | 11 |
| VSYNC | 6 |
| HREF | 7 |
| PCLK | 13 |

数据线必须严格按表连接：Y9 是 D7，Y2 是 D0，不能倒序。

## 软件版本与配置

- ESP-IDF：v5.2.7
- `espressif/esp32-camera`：2.1.7（由 Component Manager 锁定在
  `dependencies.lock`）

`sdkconfig.defaults` 配置 ESP32-S3、16 MB Quad/QIO/80 MHz Flash、8 MB
Octal/80 MHz PSRAM、PSRAM heap、Camera PSRAM DMA、240 MHz CPU 和性能优化。
PSRAM 初始化失败是致命错误，不会被忽略。

业务参数集中在 `main/main.c` 顶部：默认 VGA、JPEG 质量 14、PSRAM 双缓冲、
`CAMERA_GRAB_LATEST`。SoftAP 默认为 `H_BALL_CAM` / `hballcam`、信道 6、最多
两个 Wi-Fi 客户端；MJPEG 同时只允许一个观看客户端。

## 构建、烧录与监视

```bash
idf.py set-target esp32s3
idf.py reconfigure
idf.py build
```

使用原生 USB/JTAG 串口的示例：

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

使用 USB 转串口的示例：

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

退出监视器按 `Ctrl+]`。

## 连接与使用

电脑或手机连接 Wi-Fi `H_BALL_CAM`（密码 `hballcam`），然后访问：

- 主页：<http://192.168.4.1/>
- MJPEG 视频流：<http://192.168.4.1:81/stream>
- 单帧 JPEG：<http://192.168.4.1/capture>
- JSON 状态：<http://192.168.4.1/status>

使用 ffplay 低延迟观看：

```bash
ffplay -fflags nobuffer -flags low_delay -framedrop \
  http://192.168.4.1:81/stream
```

使用 ffmpeg 在电脑端录像为 MKV（按 `q` 停止并正确写入文件尾）：

```bash
ffmpeg -fflags nobuffer -f mjpeg -i http://192.168.4.1:81/stream \
  -c:v copy h_ball_cam.mkv
```

## 常见故障排查

### 相机未识别

确认摄像头是 OV2640、排线方向正确、3.3 V 供电稳定，并逐项核对 SCCB、XCLK、
PCLK、VSYNC、HREF 和 D7 到 D0。尤其不要把 Y9→D7 到 Y2→D0 倒序。查看启动
日志中的 `esp_camera_init()` 错误和传感器 PID。

### PSRAM 未识别

确认模组确实为 8 MB Octal PSRAM、供电为 3.3 V，并清理旧配置后重新构建：

```bash
idf.py fullclean
idf.py set-target esp32s3
idf.py reconfigure
idf.py build
```

本工程不会忽略 PSRAM 缺失；若启动阶段找不到 PSRAM，ESP-IDF 会中止，应用也
会在启动 HTTP 前再次检查。

### 花屏

优先检查 D0～D7 次序、PCLK/HREF/VSYNC 接线、杜邦线长度和接地。缩短并行数据
线，避免飞线交叉；必要时先降到 QVGA 单缓冲验证信号完整性。

### 图传卡顿

保持客户端靠近 ESP32-S3，避开拥挤的 2.4 GHz 信道，关闭同时进行的大流量
传输。可将 VGA 改为 QVGA、增大 `CAMERA_JPEG_QUALITY` 数值以降低 JPEG 大小，
或改用有线供电。固件已关闭 Wi-Fi 省电，且不会逐帧打印日志。

若 Linux 电脑使用 Realtek 等无线网卡，客户端自身的 Wi-Fi 省电也可能造成明显
卡顿。可仅对本热点连接关闭省电后重新连接：

```bash
nmcli connection modify H_BALL_CAM 802-11-wireless.powersave 2
nmcli connection up H_BALL_CAM
```

### 舵机或电机动作时 ESP32 复位

不要用 ESP32-S3 板载 3.3 V 给舵机或电机供电。使用容量足够的独立电源、共地、
就近大容量电解电容和 0.1 µF 去耦；电机需要合适的驱动器和续流保护。检查日志
中的欠压复位原因。

## 降级为 QVGA 单缓冲

在 `main/main.c` 顶部把：

```c
#define CAMERA_FRAME_SIZE   FRAMESIZE_VGA
#define CAMERA_FB_COUNT     2
```

改为：

```c
#define CAMERA_FRAME_SIZE   FRAMESIZE_QVGA
#define CAMERA_FB_COUNT     1
```

单缓冲时也可把相机配置中的抓取模式改为 `CAMERA_GRAB_WHEN_EMPTY`，进一步降低
内存压力；修改后重新执行 `idf.py build` 和烧录。
