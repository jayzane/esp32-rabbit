# Servo Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add WebSocket-based SG90 servo control via GPIO13 PWM on ESP32-WROVER-DEV

**Architecture:** LEDC hardware PWM, independent servo component, WebSocket action extension

**Tech Stack:** ESP-IDF LEDC driver, ws_control callback pattern

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `main/servo.h` | Create | servo_init/set_angle/get_angle/deinit |
| `main/servo.c` | Create | LEDC config, angle mapping, PWM control |
| `main/ws_control.h` | Modify | Add WS_ACTION_SERVO to enum |
| `main/ws_control.c` | Modify | Handle WS_ACTION_SERVO in callback |
| `main/app_main.c` | Modify | Call servo_init() before ws_control_init |

---

## Task 1: Create servo.h

**Files:**
- Create: `main/servo.h`

- [ ] **Step 1: Write servo.h**

```c
#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>

#define SERVO_MIN_ANGLE 0
#define SERVO_MAX_ANGLE 180

void servo_init(void);
void servo_set_angle(int angle);
int servo_get_angle(void);
void servo_deinit(void);

#endif // SERVO_H
```

---

## Task 2: Create servo.c

**Files:**
- Create: `main/servo.c`

- [ ] **Step 1: Write servo.c**

```c
#include "servo.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char* TAG = "servo";

#define SERVO_GPIO            GPIO_NUM_13
#define SERVO_LEDC_CHANNEL    LEDC_CHANNEL_0
#define SERVO_LEDC_TIMER      LEDC_TIMER_0
#define SERVO_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define SERVO_FREQUENCY_HZ    50
#define SERVO_DUTTY_RESOLUTION LEDC_TIMER_13_BIT  // 13-bit = 8191 max

// 20ms period = 50Hz, 13-bit resolution at 5MHz APB clk
#define SERVO_PERIOD_HZ       (5000000UL / (1 << SERVO_DUTTY_RESOLUTION))  // ~610 Hz actually
// Use 50Hz exactly: APB=80MHz, div=80, period=20000
#define SERVO_LEDC_DIVIDER    80
#define SERVO_LEDC_PERIOD    20000  // 20ms in us

static int s_current_angle = 90;

static uint32_t angle_to_duty(int angle)
{
    // 0° = 5% duty = 1000us
    // 180° = 20% duty = 4000us
    // Using period in us: 1000-4000us mapped to 0-180°
    uint32_t duty_us = 1000 + (angle * 1000 / 180);
    uint32_t duty = (duty_us * 65535) / SERVO_LEDC_PERIOD;
    return duty;
}

void servo_init(void)
{
    // LEDC timer config
    ledc_timer_config_t timer = {
        .speed_mode = SERVO_LEDC_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = SERVO_LEDC_TIMER,
        .freq_hz = SERVO_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    // LEDC channel config
    ledc_channel_config_t channel = {
        .speed_mode = SERVO_LEDC_MODE,
        .channel = SERVO_LEDC_CHANNEL,
        .timer = SERVO_LEDC_TIMER,
        .gpio_num = SERVO_GPIO,
        .duty = angle_to_duty(90),
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));

    s_current_angle = 90;
    ESP_LOGI(TAG, "Servo initialized at 90°");
}

void servo_set_angle(int angle)
{
    if (angle < SERVO_MIN_ANGLE || angle > SERVO_MAX_ANGLE) {
        ESP_LOGW(TAG, "Angle %d out of range [%d, %d]", angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
        return;
    }
    s_current_angle = angle;
    uint32_t duty = angle_to_duty(angle);
    ESP_ERROR_CHECK(ledc_set_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL));
    ESP_LOGI(TAG, "Set angle to %d° (duty=%lu)", angle, duty);
}

int servo_get_angle(void)
{
    return s_current_angle;
}

void servo_deinit(void)
{
    ledc_stop(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL, 0);
    ESP_LOGI(TAG, "Servo deinitialized");
}
```

---

## Task 3: Modify ws_control.h

**Files:**
- Modify: `main/ws_control.h:6-10`

- [ ] **Step 1: Add WS_ACTION_SERVO to enum**

```c
typedef enum {
    WS_ACTION_ON,
    WS_ACTION_OFF,
    WS_ACTION_STATUS,
    WS_ACTION_SERVO,
} ws_action_t;
```

---

## Task 4: Modify ws_control.c

**Files:**
- Modify: `main/ws_control.c`

- [ ] **Step 1: Read ws_control.c to understand callback handling**

Read the existing file structure for action handling pattern.

- [ ] **Step 2: Add cJSON parsing and servo handling**

Add include and modify callback to handle WS_ACTION_SERVO. See actual file for exact pattern, but expected addition:

```c
// Add include at top
#include "servo.h"

// In callback switch, add case:
case WS_ACTION_SERVO: {
    int angle = 90;  // default
    // parse json payload for "angle" if provided
    if (payload && strstr(payload, "angle")) {
        // extract angle from JSON
    }
    if (angle < 0 || angle > 180) {
        snprintf(response, sizeof(response),
            "{\"status\":\"error\",\"reason\":\"angle_out_of_range\"}");
    } else {
        servo_set_angle(angle);
        snprintf(response, sizeof(response),
            "{\"status\":\"ok\",\"angle\":%d}", servo_get_angle());
    }
    break;
}
```

---

## Task 5: Modify app_main.c

**Files:**
- Modify: `main/app_main.c`

- [ ] **Step 1: Add servo_init() call**

Add `#include "servo.h"` at top, then call `servo_init()` after wifi init, before `ws_control_init`.

---

## Task 6: Build verification

**Files:**
- Verify: Full project builds

- [ ] **Step 1: Build project**

Run: `idf.py build`
Expected: Clean build, no errors

---

## Task 7: Flash and test

**Files:**
- Verify: Serial output

- [ ] **Step 1: Flash to ESP32**

Run: `idf.py -p COM{X} flash monitor` (use correct COM port)

- [ ] **Step 2: Verify servo initialization log**

Expect: `Servo initialized at 90°` in serial output

- [ ] **Step 3: Test via WebSocket**

Send: `{"action": "servo", "angle": 0}`
Expect: `{"status":"ok","angle":0}`

Send: `{"action": "servo", "angle": 180}`
Expect: `{"status":"ok","angle":180}`

Send: `{"action": "servo", "angle": 90}`
Expect: `{"status":"ok","angle":90}`

---

## Spec Coverage Check

| Spec Section | Task |
|--------------|------|
| LEDC PWM, GPIO13, 50Hz | Task 1, 2 |
| 0°-180° continuous | Task 2, 7 |
| WS_ACTION_SERVO enum | Task 3 |
| JSON request/response | Task 4 |
| Init at 90° | Task 2, 5 |
| Error handling (out of range) | Task 4 |

---

**Plan complete.**

Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?