# H-BALL 电脑端图传查看器

该程序接收 ESP32-S3 + OV2640 提供的 HTTP MJPEG，完成低延迟显示、状态叠加、
截图、AVI/MJPG 录像和回放。默认由 `requests` 读取网络流并手工提取 JPEG，
OpenCV 负责解码、显示和录像，因此不依赖 OpenCV 的网络视频后端。

```text
OV2640 → ESP32-S3 → H_BALL_CAM SoftAP → HTTP MJPEG
                                      → Python/OpenCV 显示与录像
```

## 当前 Linux Python 环境

本机日常系统 Python 是 `/usr/bin/python3`（Python 3.10.12），已安装：

- OpenCV 4.5.4，带 GTK3、FFmpeg 和 GStreamer
- NumPy 1.21.5
- Requests 2.25.1
- AVI/MJPG 写入和回读已验证

ESP-IDF 的 `export.sh` 会把裸 `python3` 指向
`~/.espressif/python_env/idf5.2_py3.10_env/`。为避免把桌面依赖装进 IDF 专用
环境，下面统一显式使用 `/usr/bin/python3`。

如以后缺少依赖，可安装到当前用户的常用 Python 环境，不需要 root：

```bash
/usr/bin/python3 -m pip install --user -r requirements.txt
```

## 使用前准备

电脑连接 ESP32-S3 热点：

```text
SSID: H_BALL_CAM
密码: hballcam
```

若 Linux 无线网卡开启省电后图传卡顿，可只对本热点关闭客户端省电：

```bash
nmcli connection modify H_BALL_CAM 802-11-wireless.powersave 2
nmcli connection up H_BALL_CAM
```

程序的 Requests 会忽略桌面 HTTP/HTTPS 代理变量，确保 `192.168.4.1` 流量直接
发给开发板。默认网络连接超时为 3 秒、读取超时为 10 秒；可通过
`--connect-timeout` 和 `--read-timeout` 调整。

## 运行

基本运行：

```bash
cd pc_viewer
/usr/bin/python3 viewer.py
```

启动后自动录像：

```bash
/usr/bin/python3 viewer.py --record-on-start
```

指定视频流：

```bash
/usr/bin/python3 viewer.py \
  --stream-url http://192.168.4.1:81/stream
```

无窗口录像，适合终端或自动测试：

```bash
/usr/bin/python3 viewer.py \
  --no-display \
  --record-on-start
```

录像默认保存到 `recordings/`，格式为 AVI/MJPG。默认录像帧率为 20 FPS；程序
会按单调时钟重采样，使文件播放时长接近实际墙钟时间：

```bash
/usr/bin/python3 viewer.py --record-on-start --record-fps 20
```

默认录像不包含状态叠加；需要把叠加信息录入画面时：

```bash
/usr/bin/python3 viewer.py --record-on-start --record-overlay
```

可选 OpenCV 网络后端：

```bash
/usr/bin/python3 viewer.py --backend opencv
/usr/bin/python3 viewer.py --backend auto
```

## 快捷键

- `r`：开始或停止录像
- `s`：保存当前原始帧到 `snapshots/`
- `p`：回放最近完成的录像；录像中不可回放
- `h`：在终端打印帮助
- `q` 或 Esc：安全退出

回放时：

- 空格：暂停或继续
- `j`：后退约 5 秒
- `l`：前进约 5 秒
- `q` 或 Esc：退出回放并返回实时画面

## 独立回放

```bash
/usr/bin/python3 viewer.py --play recordings/example.avi
```

## 无硬件自测

自测不打开 GUI，也不需要连接 ESP32：

```bash
/usr/bin/python3 viewer.py --self-test
```

自测会生成合成画面和随机分块的模拟 MJPEG，检查 SOI/EOI 解析、坏数据、半帧、
缓冲区上限、JPEG 解码、AVI/MJPG 写入和回读。

## 状态接口

程序默认每 2 秒读取 `http://192.168.4.1/status`。失败不会影响图传。关闭状态
轮询：

```bash
/usr/bin/python3 viewer.py --no-status
```

## 常见问题

### 无法连接 `192.168.4.1`

确认电脑连接的是 `H_BALL_CAM`，地址应由 DHCP 分配为 `192.168.4.x`。检查：

```bash
ip -4 address
ping 192.168.4.1
curl --noproxy '*' http://192.168.4.1/status
```

### OpenCV 窗口打不开

确认当前是图形桌面会话，并检查 OpenCV GUI 后端：

```bash
/usr/bin/python3 -c "import cv2; print(cv2.getBuildInformation())"
```

远程纯终端环境使用 `--no-display --record-on-start`。

### 延迟不断增加

程序的帧队列最多保存两帧，队列满时主动丢弃最旧帧。仍有延迟时关闭 Linux
无线网卡省电，并确认没有代理接管局域网连接。

### 录像过快或过慢

使用 `--record-fps` 设置输出帧率。程序按墙钟时间定时写入，网络帧过慢时短时
重复最近帧，断线超过 2 秒后暂停写入。

### MJPG编码器不可用

先运行 `--self-test`。当前系统已验证 MJPG 可写。若更换 OpenCV 安装，检查
其 FFmpeg/GStreamer 构建信息。

### ESP32重启或推流断开

查看 ESP-IDF 串口日志，并检查摄像头排线和 3.3 V/5 V 供电。电机、舵机必须
使用容量足够的独立电源并与 ESP32 共地。

### 防火墙或NetworkManager问题

防火墙通常不会阻止主动 HTTP 连接。确认 NetworkManager 没有把默认路由或代理
错误应用到 `192.168.4.0/24`，测试时使用 `curl --noproxy '*'`。

## 可选外部检查

若系统安装了 ffprobe，可额外检查录像：

```bash
ffprobe recordings/example.avi
```

程序本身不依赖 ffprobe，录像结束时会使用 OpenCV 自动回读验证。
