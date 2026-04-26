# ESP32 摄像头监控功能设计

## 概述

为 ESP32-WROVER-DEV (OV2640) 添加摄像头监控功能，提供 WebSocket 控制接口开关摄像头，HTTP MJPEG 流输出视频。

## 架构

```
ESP32 (OV2640)
├── WebSocket Server :8080
│   └── 控制指令 (on/off/status)
└── HTTP Server :8081
    └── GET /stream → MJPEG video
```

## 硬件配置

- 开发板：ESP32-WROVER-DEV
- 摄像头：OV2640
- GPIO13：PWM 控制舵机（已存在，不影响摄像头）
- 分辨率：VGA 640x480
- 帧率：10fps

## 组件

| 组件 | 职责 |
|------|------|
| `camera_driver` | 摄像头 SCCB 初始化、帧获取、DMA 配置 |
| `camera_server` | HTTP MJPEG 流服务器 |
| `ws_control` | WebSocket 控制接口处理 |
| `camera_ctrl` | 状态机管理 (on/off) |

## WebSocket 控制协议

**连接：** `ws://<esp32_ip>:8080/control`

**客户端 → ESP32：**
```json
{"action": "on"}
{"action": "off"}
{"action": "status"}
```

**ESP32 → 客户端：**
```json
{"status": "ok", "camera": "on"}
{"status": "ok", "camera": "off"}
{"status": "error", "reason": "init_failed"}
```

## 视频流

**端点：** `GET http://<esp32_ip>:8081/stream`

**返回：** multipart/x-mixed-replace MIME 类型，每帧 JPEG

**浏览器访问：** `<img src="http://<esp32_ip>:8081/stream" />`

## 行为

- 上电后 WiFi 连接完成自动开启摄像头并开始推流
- WS 收到 `off` → 停止摄像头，回应状态
- WS 收到 `on` → 重启摄像头，回应状态
- WS 收到 `status` → 回当前状态

## 错误处理

| 场景 | 处理 |
|------|------|
| 摄像头初始化失败 | WS 返回 error，reason=init_failed |
| 运行中摄像头故障 | 自动重试 3 次，失败后通知 error |
| WiFi 断开 | 暂停推流，重连后自动恢复 |

## 文件结构

```
main/
├── camera_driver.c/h    # OV2640 驱动封装
├── camera_server.c/h    # HTTP MJPEG 流
├── ws_control.c/h       # WebSocket 控制处理
├── camera_ctrl.c/h      # 状态管理
└── app_main.c           # 入口，组件初始化
```

## 依赖

- esp32-camera (ESP-IDF component)
- libwebsockets (WebSocket)
- esp_http_server (MJPEG stream)
