# ESP32 双核架构重构设计

## 概述

将 ESP32 摄像头系统从单核改为双核分工：CORE 1 专职摄像头驱动，CORE 0 负责 WiFi + WebSocket 客户端 + 舵机 + 业务逻辑。ESP32 从 HTTP 服务器改为主动连接服务器的 WebSocket 客户端。

## 网络拓扑

```
                    ┌─────────────────────────────────────────────┐
                    │              10.0.0.232                      │
                    │         (WebSocket 服务器)                   │
                    │                                             │
                    │  <-- JSON 命令  {cmd:"camera_on"}           │
                    │  <-- 二进制 JPEG  (WebSocket binary frame)  │
                    └─────────── WebSocket 连接 ──────────────────┘

                    ┌──────── CORE 0 ────────────────────────────┐
                    │  • WiFi STA                                   │
                    │  • WebSocket 客户端                          │
                    │  • 命令解析 + 业务逻辑                        │
                    │  • 舵机 PWM 控制                              │
                    │  • CORE 1 通信 (Queue)                       │
                    └────────┬────────────────────────────────────┘
                             │ Queue: cmd + fb pointer
                             ▼
                    ┌──────── CORE 1 ────────────────────────────┐
                    │  • esp32-camera 驱动                       │
                    │  • 帧捕获 (按需，非连续)                    │
                    │  • PSRAM 共享缓冲区存 JPEG                 │
                    └────────────────────────────────────────────┘
```

## 硬件配置

- 开发板：ESP32-WROVER-DEV (4MB PSRAM)
- 摄像头：OV2640
- GPIO13：PWM 舵机 (SG90)
- 分辨率：QVGA 320x240（默认），VGA 640x480（可选）
- JPEG buf：PSRAM 128KB 共享缓冲区

## 双核分工

### CORE 1 — 摄像头任务

`camera_task.c` 纯被动，只处理 Queue 命令：

- `CAMERA_INIT` → `esp_camera_init`
- `CAMERA_CAPTURE` → `esp_camera_fb_get` → JPEG 写入 PSRAM 共享 buf → 通知 CORE 0
- `CAMERA_DEINIT` → `esp_camera_deinit`

esp32-camera 内部 DMA 绑定 CPU CORE 1，无需额外 pin。

### CORE 0 — 业务任务

`app_main` + `ws_client_task`：

- WiFi STA 连接管理
- WebSocket 客户端（连接 10.0.0.232），断线重连
- 命令解析和分发
- 舵机 PWM 控制

## CORE 0 ↔ CORE 1 通信

### 队列消息

```c
typedef enum {
    CAMERA_NONE,
    CAMERA_INIT,
    CAMERA_DEINIT,
    CAMERA_CAPTURE
} camera_cmd_t;

typedef struct {
    camera_cmd_t cmd;
    uint32_t seq;
    size_t frame_len;   // CORE 1 填充
} camera_msg_t;
```

### 共享 PSRAM 缓冲区

```c
// shared_mem.c
#ifdef CONFIG_ESP32_SPIRAM
    static uint8_t jpeg_buf[JPEG_BUF_SIZE] __attribute__((section(".psram")));
#else
    static DRAM_ATTR uint8_t jpeg_buf[JPEG_BUF_SIZE];  // DRAM fallback
#endif
```

`JPEG_BUF_SIZE` = 128KB（足够 VGA JPEG）

## 通信协议

### 服务器 → ESP32 (WebSocket TEXT)

```json
{"cmd": "camera_on"}
{"cmd": "camera_off"}
{"cmd": "servo", "angle": 90}
{"cmd": "capture"}
{"cmd": "stream_start"}
{"cmd": "stream_stop"}
```

### ESP32 → 服务器 (TEXT 响应)

```json
{"seq": 42, "status": "ok", "camera": "on"}
{"seq": 43, "status": "ok", "angle": 90}
{"seq": 44, "error": "camera_init_failed"}
```

### ESP32 → 服务器 (BINARY)

- JPEG 原始二进制帧（ws_sendbinary）
- 流式连续帧：`[4B length][JPEG...][4B length][JPEG...]`

### 心跳

WebSocket Ping/Pong，30s 超时触发重连。

## 文件变更

| 操作 | 文件 |
|------|------|
| 新增 | `main/camera_task.c/h` — CORE 1 摄像头任务 |
| 新增 | `main/shared_mem.c/h` — PSRAM 共享 buf + Queue 定义 |
| 新增 | `main/ws_client.c/h` — WebSocket 客户端 |
| 重构 | `main/camera_driver.c/h` — 移除 server，只保留 init/deinit/capture |
| 重构 | `main/camera_ctrl.c/h` — Queue 驱动状态机 |
| 删除 | `main/camera_server.c/h` — MJPEG HTTP stream |
| 重构 | `main/app_main.c` — CORE 1 启动 + ws_client 初始化 |
| 删除 | `main/ws_control.c/h` — 被 ws_client 替换 |

## 错误处理

| 场景 | 处理 |
|------|------|
| WiFi 断开 | CORE 0 监听断网事件，触发 WS 重连 + CORE 1 camera deinit |
| WebSocket 断开 | 指数退避重连：1s → 2s → 4s → ... → max 30s |
| CORE 1 camera 故障 | CORE 0 收到错误，通知服务器，deinit CORE 1 task |
| JPEG 帧 > PSRAM buf | quality 降低重试 |
| Queue 满 | 覆盖模式，新命令优先 |
| esp_camera_init 失败 | 返回错误码，通知服务器 |

## 行为变更

- 上电后不自动开摄像头，等服务器发 `camera_on`
- 图像不再连续推流，改为按需采集（`capture` 单帧，`stream_start` 连续）
- 服务器主动控制一切：开关摄像头、调整舵机、采集图像
