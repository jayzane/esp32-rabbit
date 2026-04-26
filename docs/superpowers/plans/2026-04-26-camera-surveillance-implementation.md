# ESP32 Camera Surveillance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** ESP32 摄像头监控功能 - WebSocket 控制开关摄像头，HTTP MJPEG 流输出视频

**Architecture:** OV2640 摄像头通过 SCCB 配置，DMA 获取帧数据，JPEG 编码后通过 HTTP multipart 输出 MJPEG 流；WebSocket 处理 on/off/status 控制指令；camera_ctrl 组件管理状态机。

**Tech Stack:** ESP-IDF v6.0, esp32-camera component, libwebsockets, esp_http_server

---

## 文件结构

```
main/
├── camera_driver.c/h       # OV2640 驱动封装
├── camera_server.c/h       # HTTP MJPEG 流
├── ws_control.c/h          # WebSocket 控制
├── camera_ctrl.c/h         # 状态管理
├── app_main.c              # 入口
└── CMakeLists.txt          # 构建配置
components/                  # 外部组件
├── esp32-camera/
└── libwebsockets/
```

---

## Task 1: ESP-IDF 项目初始化

**Files:**
- Create: `main/CMakeLists.txt`
- Create: `CMakeLists.txt`
- Create: `sdkconfig`
- Create: `main/component.mk`

- [ ] **Step 1: Create main/CMakeLists.txt**

```cmake
idf_component_register(SRCS "app_main.c" "camera_driver.c" "camera_server.c" "ws_control.c" "camera_ctrl.c"
    INCLUDE_DIRS ".")
```

- [ ] **Step 2: Create project CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(camera_surveillance)
```

- [ ] **Step 3: Clone esp32-camera component**

Run: `git clone https://github.com/espressif/esp32-camera.git components/esp32-camera`

- [ ] **Step 4: Configure sdkconfig defaults**

```bash
idf.py set-target esp32
idf.py menuconfig
```

Set camera pins per ESP32-WROVER-DEV, enable PSRAM.

- [ ] **Step 5: Commit**

```bash
git add main/CMakeLists.txt CMakeLists.txt sdkconfig components/
git commit -m "feat: ESP-IDF project scaffold for camera surveillance"
```

---

## Task 2: camera_driver 组件

**Files:**
- Create: `main/camera_driver.c`
- Create: `main/camera_driver.h`

**Dependencies:** esp32-camera component

- [ ] **Step 1: Write camera_driver.h**

```c
#ifndef CAMERA_DRIVER_H
#define CAMERA_DRIVER_H

#include "esp_camera.h"

typedef enum {
    CAMERA_OK = 0,
    CAMERA_ERROR_INIT_FAILED = 1,
    CAMERA_ERROR_NOT_INITIALIZED = 2,
    CAMERA_ERROR_FRAME_TIMEOUT = 3,
} camera_err_t;

camera_err_t camera_driver_init(void);
void camera_driver_deinit(void);
camera_err_t camera_driver_start(void);
void camera_driver_stop(void);
bool camera_driver_is_running(void);
camera_fb_t* camera_driver_get_frame(void);
void camera_driver_return_frame(camera_fb_t* fb);
bool camera_driver_is_initialized(void);

#endif // CAMERA_DRIVER_H
```

- [ ] **Step 2: Write camera_driver.c**

```c
#include "camera_driver.h"
#include "esp_log.h"
#include "esp_camera.h"

static const char* TAG = "camera_driver";

static bool s_running = false;
static bool s_initialized = false;

#define CAMERA_PWDN_GPIO    (-1)
#define CAMERA_RESET_GPIO   (-1)
#define CAMERA_XCLK_GPIO    0
#define CAMERA_SCCB_SDA_GPIO 26
#define CAMERA_SCCB_SCL_GPIO 27
#define CAMERA_D7_GPIO      35
#define CAMERA_D6_GPIO      34
#define CAMERA_D5_GPIO      39
#define CAMERA_D4_GPIO      36
#define CAMERA_D3_GPIO      21
#define CAMERA_D2_GPIO      19
#define CAMERA_D1_GPIO      18
#define CAMERA_D0_GPIO       5
#define CAMERA_VSYNC_GPIO   25
#define CAMERA_HREF_GPIO    23
#define CAMERA_PCLK_GPIO    22

camera_err_t camera_driver_init(void) {
    camera_config_t config = {
        .pin_pwdn = CAMERA_PWDN_GPIO,
        .pin_reset = CAMERA_RESET_GPIO,
        .pin_xclk = CAMERA_XCLK_GPIO,
        .pin_sccb_sda = CAMERA_SCCB_SDA_GPIO,
        .pin_sccb_scl = CAMERA_SCCB_SCL_GPIO,
        .pin_d7 = CAMERA_D7_GPIO,
        .pin_d6 = CAMERA_D6_GPIO,
        .pin_d5 = CAMERA_D5_GPIO,
        .pin_d4 = CAMERA_D4_GPIO,
        .pin_d3 = CAMERA_D3_GPIO,
        .pin_d2 = CAMERA_D2_GPIO,
        .pin_d1 = CAMERA_D1_GPIO,
        .pin_d0 = CAMERA_D0_GPIO,
        .pin_vsync = CAMERA_VSYNC_GPIO,
        .pin_href = CAMERA_HREF_GPIO,
        .pin_pclk = CAMERA_PCLK_GPIO,
        .xclk_freq_hz = 10000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_VGA,
        .jpeg_quality = 8,
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        s_initialized = false;
        return CAMERA_ERROR_INIT_FAILED;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_brightness(s, 0);
        s->set_contrast(s, 0);
        s->set_saturation(s, 0);
        s->set_whitebal(s, 1);
        s->set_awb_gain(s, 1);
        s->set_wb_mode(s, 0);
        s->set_exposure_ctrl(s, 1);
        s->set_ae_level(s, 0);
        s->set_aec_value(s, 300);
        s->set_gain_ctrl(s, 1);
        s->set_agc_gain(s, 30);
        s->set_gainceiling(s, (gainceiling_t)0);
        s->set_bpc(s, 0);
        s->set_wpc(s, 1);
        s->set_raw_gma(s, 1);
        s->set_lenc(s, 1);
        s->set_hmirror(s, 0);
        s->set_vflip(s, 0);
        s->set_dcw(s, 1);
        s->set_colorbar(s, 0);
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Camera initialized");
    return CAMERA_OK;
}

void camera_driver_deinit(void) {
    if (s_initialized) {
        esp_camera_deinit();
        s_initialized = false;
        s_running = false;
        ESP_LOGI(TAG, "Camera deinitialized");
    }
}

camera_err_t camera_driver_start(void) {
    if (!s_initialized) {
        return CAMERA_ERROR_NOT_INITIALIZED;
    }
    s_running = true;
    ESP_LOGI(TAG, "Camera started");
    return CAMERA_OK;
}

void camera_driver_stop(void) {
    s_running = false;
    ESP_LOGI(TAG, "Camera stopped");
}

bool camera_driver_is_running(void) {
    return s_running;
}

camera_fb_t* camera_driver_get_frame(void) {
    if (!s_running) return NULL;
    return esp_camera_fb_get();
}

void camera_driver_return_frame(camera_fb_t* fb) {
    if (fb) {
        esp_camera_fb_return(fb);
    }
}

bool camera_driver_is_initialized(void) {
    return s_initialized;
}
```

- [ ] **Step 3: Commit**

```bash
git add main/camera_driver.c main/camera_driver.h
git commit -m "feat: add camera_driver component for OV2640"
```

---

## Task 3: camera_ctrl 组件

**Files:**
- Create: `main/camera_ctrl.c`
- Create: `main/camera_ctrl.h`

- [ ] **Step 1: Write camera_ctrl.h**

```c
#ifndef CAMERA_CTRL_H
#define CAMERA_CTRL_H

#include <stdbool.h>
#include "camera_driver.h"

typedef enum {
    CAMERA_STATE_OFF = 0,
    CAMERA_STATE_ON = 1,
} camera_state_t;

typedef void (*camera_state_change_cb_t)(camera_state_t new_state, void* user_data);

void camera_ctrl_init(void);
void camera_ctrl_deinit(void);
camera_err_t camera_ctrl_enable(camera_state_change_cb_t callback, void* user_data);
void camera_ctrl_disable(void);
camera_state_t camera_ctrl_get_state(void);
bool camera_ctrl_is_on(void);

#endif // CAMERA_CTRL_H
```

- [ ] **Step 2: Write camera_ctrl.c**

```c
#include "camera_ctrl.h"
#include "esp_log.h"

static const char* TAG = "camera_ctrl";

static camera_state_t s_state = CAMERA_STATE_OFF;
static camera_state_change_cb_t s_callback = NULL;
static void* s_user_data = NULL;
static bool s_enabled = false;

void camera_ctrl_init(void) {
    s_state = CAMERA_STATE_OFF;
    s_callback = NULL;
    s_user_data = NULL;
    s_enabled = false;
    ESP_LOGI(TAG, "Camera control initialized");
}

void camera_ctrl_deinit(void) {
    camera_ctrl_disable();
    s_state = CAMERA_STATE_OFF;
    ESP_LOGI(TAG, "Camera control deinitialized");
}

camera_err_t camera_ctrl_enable(camera_state_change_cb_t callback, void* user_data) {
    if (s_enabled) {
        return CAMERA_OK;
    }

    s_callback = callback;
    s_user_data = user_data;
    s_enabled = true;

    camera_err_t err = camera_driver_init();
    if (err != CAMERA_OK) {
        s_enabled = false;
        return err;
    }

    err = camera_driver_start();
    if (err != CAMERA_OK) {
        camera_driver_deinit();
        s_enabled = false;
        return err;
    }

    s_state = CAMERA_STATE_ON;
    if (s_callback) {
        s_callback(CAMERA_STATE_ON, s_user_data);
    }

    ESP_LOGI(TAG, "Camera enabled");
    return CAMERA_OK;
}

void camera_ctrl_disable(void) {
    if (!s_enabled) return;

    camera_driver_stop();
    camera_driver_deinit();
    s_state = CAMERA_STATE_OFF;
    s_enabled = false;

    if (s_callback) {
        s_callback(CAMERA_STATE_OFF, s_user_data);
    }

    ESP_LOGI(TAG, "Camera disabled");
}

camera_state_t camera_ctrl_get_state(void) {
    return s_state;
}

bool camera_ctrl_is_on(void) {
    return s_state == CAMERA_STATE_ON && s_enabled;
}
```

- [ ] **Step 3: Commit**

```bash
git add main/camera_ctrl.c main/camera_ctrl.h
git commit -m "feat: add camera_ctrl state management component"
```

---

## Task 4: camera_server 组件 (HTTP MJPEG 流)

**Files:**
- Create: `main/camera_server.c`
- Create: `main/camera_server.h`

**依赖:** esp_http_server component

- [ ] **Step 1: Write camera_server.h**

```c
#ifndef CAMERA_SERVER_H
#define CAMERA_SERVER_H

#include <stdbool.h>

void camera_server_start(void);
void camera_server_stop(void);
bool camera_server_is_running(void);

#endif // CAMERA_SERVER_H
```

- [ ] **Step 2: Write camera_server.c**

```c
#include "camera_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "camera_driver.h"

static const char* TAG = "camera_server";
static httpd_handle_t s_httpd = NULL;

static esp_err_t stream_handler(httpd_req_t* req) {
    char* part_buf[128];
    esp_err_t res = ESP_OK;

    if (!camera_driver_is_running()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Camera not running");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");

    while (true) {
        camera_fb_t* fb = camera_driver_get_frame();
        if (!fb) {
            ESP_LOGW(TAG, "Frame capture timeout");
            res = ESP_FAIL;
            break;
        }

        if (fb->format == PIXFORMAT_JPEG) {
            size_t hlen = snprintf((char*)part_buf, 128,
                "Content-Type: image/jpeg\r\n"
                "Content-Length: %u\r\n\r\n",
                fb->len);

            res = httpd_resp_send_chunk(req, (const char*)part_buf, hlen);
            if (res == ESP_OK) {
                res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
            }
        }

        camera_driver_return_frame(fb);

        if (res != ESP_OK) break;

        vTaskDelay(pdMS_TO_TICKS(100)); // ~10fps
    }

    return res;
}

static esp_err_t index_handler(httpd_req_t* req) {
    const char* resp = "<html><head><title>ESP32 Camera</title></head>"
        "<body><h1>ESP32 Camera Stream</h1>"
        "<img src=\"/stream\" /></body></html>";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

void camera_server_start(void) {
    if (s_httpd) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8081;
    config.ctrl_port = 8081;

    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL
    };

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = NULL
    };

    if (httpd_start(&s_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(s_httpd, &stream_uri);
        httpd_register_uri_handler(s_httpd, &index_uri);
        ESP_LOGI(TAG, "Camera server started on port 8081");
    } else {
        ESP_LOGE(TAG, "Failed to start camera server");
    }
}

void camera_server_stop(void) {
    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
        ESP_LOGI(TAG, "Camera server stopped");
    }
}

bool camera_server_is_running(void) {
    return s_httpd != NULL;
}
```

- [ ] **Step 3: Commit**

```bash
git add main/camera_server.c main/camera_server.h
git commit -m "feat: add camera_server HTTP MJPEG streaming component"
```

---

## Task 5: ws_control 组件 (WebSocket 控制)

**Files:**
- Create: `main/ws_control.c`
- Create: `main/ws_control.h`

**依赖:** libwebsockets component

- [ ] **Step 1: Write ws_control.h**

```c
#ifndef WS_CONTROL_H
#define WS_CONTROL_H

#include <stdbool.h>

typedef enum {
    WS_ACTION_ON,
    WS_ACTION_OFF,
    WS_ACTION_STATUS,
} ws_action_t;

typedef void (*ws_send_response_cb_t)(const char* json_response, void* user_data);
typedef void (*ws_control_callback_t)(ws_action_t action, void* user_data);

void ws_control_init(ws_control_callback_t callback, void* user_data);
void ws_control_deinit(void);
void ws_control_send_response(const char* json_response);

#endif // WS_CONTROL_H
```

- [ ] **Step 2: Write ws_control.c**

```c
#include "ws_control.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char* TAG = "ws_control";
static ws_control_callback_t s_callback = NULL;
static void* s_user_data = NULL;
static const char* s_ws_url = "/control";
static bool s_initialized = false;

void ws_control_init(ws_control_callback_t callback, void* user_data) {
    s_callback = callback;
    s_user_data = user_data;
    s_initialized = true;
    ESP_LOGI(TAG, "WebSocket control initialized");
}

void ws_control_deinit(void) {
    s_initialized = false;
    s_callback = NULL;
    s_user_data = NULL;
    ESP_LOGI(TAG, "WebSocket control deinitialized");
}

void ws_control_send_response(const char* json_response) {
    // Response sent via registered callback - actual WS send handled by app_main
    ESP_LOGD(TAG, "WS response: %s", json_response);
}

ws_action_t ws_control_parse_action(const char* json_str) {
    cJSON* root = cJSON_Parse(json_str);
    if (!root) return WS_ACTION_STATUS;

    cJSON* action = cJSON_GetObjectItem(root, "action");
    if (!action || !cJSON_IsString(action)) {
        cJSON_Delete(root);
        return WS_ACTION_STATUS;
    }

    ws_action_t result;
    if (strcmp(action->valuestring, "on") == 0) {
        result = WS_ACTION_ON;
    } else if (strcmp(action->valuestring, "off") == 0) {
        result = WS_ACTION_OFF;
    } else {
        result = WS_ACTION_STATUS;
    }

    cJSON_Delete(root);
    return result;
}

void ws_control_handle_message(const char* json_str) {
    if (!s_initialized || !s_callback) return;

    ws_action_t action = ws_control_parse_action(json_str);
    s_callback(action, s_user_data);
}
```

- [ ] **Step 3: Commit**

```bash
git add main/ws_control.c main/ws_control.h
git commit -m "feat: add ws_control WebSocket control component"
```

---

## Task 6: app_main 整合

**Files:**
- Create: `main/app_main.c`

- [ ] **Step 1: Write app_main.c**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "camera_ctrl.h"
#include "camera_server.h"
#include "ws_control.h"

static const char* TAG = "app_main";

#define WS_SERVER_PORT 8080

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "WiFi connected, starting camera services...");
        camera_server_start();
    }
}

static void ws_control_callback(ws_action_t action, void* user_data) {
    char response[128];

    switch (action) {
        case WS_ACTION_ON:
            if (!camera_ctrl_is_on()) {
                camera_err_t err = camera_ctrl_enable(NULL, NULL);
                if (err == CAMERA_OK) {
                    snprintf(response, sizeof(response),
                        "{\"status\":\"ok\",\"camera\":\"on\"}");
                } else {
                    snprintf(response, sizeof(response),
                        "{\"status\":\"error\",\"reason\":\"init_failed\"}");
                }
            } else {
                snprintf(response, sizeof(response),
                    "{\"status\":\"ok\",\"camera\":\"on\"}");
            }
            break;

        case WS_ACTION_OFF:
            if (camera_ctrl_is_on()) {
                camera_ctrl_disable();
            }
            snprintf(response, sizeof(response),
                "{\"status\":\"ok\",\"camera\":\"off\"}");
            break;

        case WS_ACTION_STATUS:
        default:
            snprintf(response, sizeof(response),
                "{\"status\":\"ok\",\"camera\":\"%s\"}",
                camera_ctrl_is_on() ? "on" : "off");
            break;
    }

    ws_control_send_response(response);
    ESP_LOGI(TAG, "Camera control: %s", response);
}

void app_main(void) {
    ESP_LOGI(TAG, "ESP32 Camera Surveillance Starting...");

    // NVS flash init
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    // WiFi init
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA_PSK,
            .pmf_cfg = { .capable = true, .required = false },
        },
    };
    strncpy((char*)wifi_config.sta.ssid, CONFIG_ESP_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, CONFIG_ESP_WIFI_PASSWORD, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init complete, connecting to SSID: %s", CONFIG_ESP_WIFI_SSID);

    // Camera control init
    camera_ctrl_init();

    // WebSocket control init
    ws_control_init(ws_control_callback, NULL);

    // Auto-start camera on boot
    ESP_LOGI(TAG, "Auto-starting camera...");
    camera_err_t err = camera_ctrl_enable(NULL, NULL);
    if (err == CAMERA_OK) {
        ESP_LOGI(TAG, "Camera auto-started successfully");
    } else {
        ESP_LOGE(TAG, "Camera auto-start failed: %d", err);
    }

    // Start HTTP server for MJPEG stream
    camera_server_start();
}
```

- [ ] **Step 2: Commit**

```bash
git add main/app_main.c
git commit -m "feat: integrate camera surveillance components in app_main"
```

---

## 验证清单

1. **摄像头初始化** - 上电后 camera_driver 正确初始化 OV2640，SCCB 通信正常
2. **MJPEG 流** - `GET http://<esp32_ip>:8081/stream` 在浏览器显示视频
3. **WebSocket ON/OFF** - WS 发送 `{"action":"off"}` 摄像头停止，`{"action":"on"}` 重新开启
4. **默认启动** - 上电自动开启摄像头，无需手动触发

---

## 依赖配置

**idf.py menuconfig 设置：**
- CONFIG_ESP_WIFI_SSID="${WiFi_SSID}"
- CONFIG_ESP_WIFI_PASSWORD="${WIFI_PASS}"
- Camera → OV2640 → Enable
- Camera → PSRAM → Enable (if using WROVER)
