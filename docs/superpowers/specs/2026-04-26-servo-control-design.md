# Servo Control Interface Design

## Overview

Add WebSocket-based servo control to ESP32-WROVER-DEV. SG90舵机通过GPIO13 PWM控制。

## Hardware

- GPIO13 PWM (LEDC channel 0)
- SG90 舵机：50Hz，0°-180°，20ms周期
- 占空比：5%(0°) - 20%(180°)，16-bit精度

## Architecture

```
ws_control.c → servo_set_angle() → LEDC硬件PWM
```

独立组件 `servo.c/h`，不依赖camera系统。

## Implementation

### 角度映射

| 角度 | 占空比 | LEDC值(16-bit) |
|------|--------|----------------|
| 0°   | 5%     | 1638           |
| 180° | 20%    | 8192           |

公式：`ledc_value = (angle / 180.0 * 6554) + 1638`

### 接口

```c
// main/servo.h
void servo_init(void);
void servo_set_angle(int angle);
int servo_get_angle(void);
void servo_deinit(void);
```

### WebSocket 扩展

```c
// ws_control.h
typedef enum {
    WS_ACTION_ON,
    WS_ACTION_OFF,
    WS_ACTION_STATUS,
    WS_ACTION_SERVO,
} ws_action_t;
```

```json
// 请求 {"action": "servo", "angle": 90}
// 响应 {"status": "ok", "angle": 90}
```

### 初始化流程

`app_main`:
1. `servo_init()` → 归中 90°
2. `ws_control_init(ws_control_callback, NULL)`
3. `ws_control_callback` 处理 `WS_ACTION_SERVO`

### 错误处理

- 角度范围检查：0-180°
- 超范围返回：`{"status": "error", "reason": "angle_out_of_range"}`

## Files

- `main/servo.h` - 头文件
- `main/servo.c` - 实现
- `main/ws_control.h` - 修改：添加 WS_ACTION_SERVO
- `main/ws_control.c` - 修改：处理 servo action
- `main/app_main.c` - 修改：调用 servo_init()

## Testing

1. 串口日志验证初始化角度
2. WebSocket 发送 servo 命令，验证角度响应
3. 边界测试：-1°, 181° 应返回 error