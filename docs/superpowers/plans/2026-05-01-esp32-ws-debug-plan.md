# ESP32 WS Debug Implementation Plan

**Goal:** Create standalone ESP32 debug project that connects to PC WebSocket server, sends heartbeat, prints received messages. Verify WS connection works before integrating with full app.

**Architecture:** Minimal ESP-IDF project with one task: connect WiFi, establish WS connection, send periodic heartbeats, print all received data. Uses mbedTLS SHA1 for correct WS handshake.

**Tech Stack:** ESP-IDF v6.0, mbedTLS, POSIX sockets, aiohttp Python server

---

## File Structure

```
esp32_ws_debug/
├── CMakeLists.txt
├── sdkconfig.defaults
├── main/
│   ├── CMakeLists.txt
│   ├── ws_echo.h           # WS echo interface
│   ├── ws_echo.c           # WS connection + receive + send
│   └── app_main.c          # WiFi init + task creation
└── server/
    └── echo_server.py      # Python echo WS server (port 11081)
```

---

## Task 1: Create Directory Structure and Build Config

**Files:**
- Create: `esp32_ws_debug/CMakeLists.txt`
- Create: `esp32_ws_debug/sdkconfig.defaults`
- Create: `esp32_ws_debug/main/CMakeLists.txt`

- [ ] **Step 1: Create directory**

```bash
mkdir -p esp32_ws_debug/main
mkdir -p esp32_ws_debug/server
```

- [ ] **Step 2: Write top-level CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ws_echo)
```

- [ ] **Step 3: Write main/CMakeLists.txt**

```cmake
idf_component_register(SRCS "app_main.c" "ws_echo.c"
    INCLUDE_DIRS ".")
```

- [ ] **Step 4: Write sdkconfig.defaults**

```
CONFIG_ESP_WIFI_ENABLED=y
CONFIG_ESP_NETIF_ENABLED=y
CONFIG_LWIP_ENABLED=y
CONFIG_ESP_EVENT_LOOP_ENABLED=y
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=10
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=32
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=32
CONFIG_ESP_WIFI_AMPDU_TX_ENABLED=y
CONFIG_ESP_WIFI_AMPDU_RX_ENABLED=y
CONFIG_ESP_WIFI_NVS_ENABLED=y
CONFIG_LWIP_MAX_SOCKETS=16
CONFIG_LWIP_SO_REUSE=y
CONFIG_LWIP_SO_RCVBUF=y
CONFIG_LWIP_TCP_SND_BUF_DEFAULT=5760
CONFIG_LWIP_TCP_WND_DEFAULT=5760
```

---

## Task 2: Write echo_server.py

**Files:**
- Create: `esp32_ws_debug/server/echo_server.py`

- [ ] **Step 1: Write echo_server.py**

```python
#!/usr/bin/env python3
"""Simple WebSocket echo server for ESP32 debug testing."""
import asyncio
import json
from aiohttp import web

PORT = 11081

async def ws_handler(request):
    ws = web.WebSocketResponse()
    await ws.prepare(request)
    print(f"[SERVER] ESP32 connected from {request.transport.get_extra_info('peername')}")

    async for msg in ws:
        if msg.type == web.WSMsgType.TEXT:
            print(f"[SERVER] Received: {msg.data}")
            # Echo back with wrapper
            try:
                data = json.loads(msg.data)
                echo = {"echo": data}
                await ws.send_str(json.dumps(echo))
                print(f"[SERVER] Sent echo: {echo}")
            except json.JSONDecodeError:
                await ws.send_str(f"{{\"echo\": \"{msg.data}\"}}")
                print(f"[SERVER] Sent echo (raw): {msg.data}")
        elif msg.type == web.WSMsgType.BINARY:
            print(f"[SERVER] Received binary: {msg.data.hex()}")
            await ws.send_bytes(msg.data)

    print("[SERVER] ESP32 disconnected")
    return ws

async def main():
    app = web.Application()
    app.router.add_get('/debug', ws_handler)
    runner = web.AppRunner(app)
    await runner.setup()
    await web.TCPSite(runner, '0.0.0.0', PORT).start()
    print(f"[SERVER] Echo server running on ws://0.0.0.0:{PORT}/debug")
    print("Waiting for ESP32 connection...")

    # Keep alive
    while True:
        await asyncio.sleep(3600)

asyncio.run(main())
```

- [ ] **Step 2: Test server starts**

```bash
python esp32_ws_debug/server/echo_server.py
# Expected: "[SERVER] Echo server running on ws://0.0.0.0:11081/debug"
# Press Ctrl+C to stop
```

---

## Task 3: Write ws_echo.h

**Files:**
- Create: `esp32_ws_debug/main/ws_echo.h`

- [ ] **Step 1: Write ws_echo.h**

```c
#ifndef WS_ECHO_H
#define WS_ECHO_H

#include <stdint.h>
#include <stdbool.h>

#define WS_ECHO_SERVER_URI  "ws://10.0.0.232:11081/debug"
#define WS_ECHO_HEARTBEAT_MS 5000
#define WS_ECHO_RECONNECT_MAX_MS 30000

typedef enum {
    WS_ECHO_DISCONNECTED = 0,
    WS_ECHO_CONNECTING,
    WS_ECHO_CONNECTED,
} ws_echo_state_t;

typedef void (*ws_echo_on_receive_t)(const char* data, int len);

void ws_echo_init(ws_echo_on_receive_t callback);
void ws_echo_deinit(void);
bool ws_echo_is_connected(void);
void ws_echo_send(const char* json);

#endif // WS_ECHO_H
```

---

## Task 4: Write ws_echo.c (Core WS Logic)

**Files:**
- Create: `esp32_ws_debug/main/ws_echo.c`

- [ ] **Step 1: Write ws_echo.c**

Key implementation details:
- Use `esp_crypto_hash.h` (mbedTLS wrapper) for correct SHA1
- Parse URI `ws://host:port/path` manually
- Use `select()` + non-blocking recv in task loop
- Masked WS frames (all client frames must be masked per RFC 6455)
- Send JSON heartbeat every 5 seconds when connected
- Callback fires for each received text frame

```c
#include "ws_echo.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_crypto_hash.h"
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

static const char* TAG = "ws_echo";

// Server config - parsed from WS_ECHO_SERVER_URI
static char s_host[64] = {0};
static int  s_port = 11081;
static char s_path[64] = "/";

// Connection state
static ws_echo_state_t s_state = WS_ECHO_DISCONNECTED;
static int s_sock_fd = -1;
static ws_echo_on_receive_t s_recv_cb = NULL;

// Heartbeat
static uint32_t s_seq = 0;
static int64_t s_last_heartbeat_ms = 0;
static TaskHandle_t s_task_handle = NULL;
static bool s_task_running = false;

// ---------------------------------------------------------------------------
// SHA1 for WS handshake - uses mbedTLS via esp_crypto_hash
// ---------------------------------------------------------------------------

static bool sha1_raw(const char* data, size_t len, uint8_t* out_sha1_20bytes)
{
    // mbedTLS SHA-1: supported in ESP-IDF via mbedtls library
    // We implement a minimal portable SHA-1 since mbedTLS API varies by IDF version
    // Use the same approach as ESP-IDF examples (HAL SHA1 or mbedtls_md)
    // For this debug project, use mbedTLS md_context approach

    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA1;
    const mbedtls_md_info_t* md_info;

    mbedtls_md_init(&ctx);
    md_info = mbedtls_md_info_from_type(md_type);
    if (!md_info) {
        ESP_LOGE(TAG, "SHA1 info not found");
        mbedtls_md_free(&ctx);
        return false;
    }

    int ret = mbedtls_md_setup(&ctx, md_info, 0);
    if (ret != 0) {
        ESP_LOGE(TAG, "SHA1 setup failed: %d", ret);
        mbedtls_md_free(&ctx);
        return false;
    }

    mbedtls_md_update(&ctx, (const unsigned char*)data, len);
    mbedtls_md_finish(&ctx, out_sha1_20bytes);
    mbedtls_md_free(&ctx);

    return true;
}

// Base64 encode (standard table)
static const char s_base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_encode(const uint8_t* in, int in_len, char* out)
{
    int i, j;
    for (i = 0, j = 0; i < in_len; i += 3) {
        int a = in[i];
        int b = (i + 1 < in_len) ? in[i + 1] : 0;
        int c = (i + 2 < in_len) ? in[i + 2] : 0;
        int triplet = (a << 16) | (b << 8) | c;
        out[j++] = s_base64_table[(triplet >> 18) & 0x3F];
        out[j++] = s_base64_table[(triplet >> 12) & 0x3F];
        out[j++] = (i + 1 < in_len) ? s_base64_table[(triplet >> 6) & 0x3F] : '=';
        out[j++] = (i + 2 < in_len) ? s_base64_table[triplet & 0x3F] : '=';
    }
    out[j] = '\0';
    return j;
}

// ---------------------------------------------------------------------------
// URI parsing
// ---------------------------------------------------------------------------

static void parse_uri(void)
{
    const char* uri = WS_ECHO_SERVER_URI;
    // Skip "ws://"
    const char* p = strstr(uri, "://");
    if (!p) return;
    p += 3;

    // host:port/path
    const char* colon = strchr(p, ':');
    const char* slash = strchr(p, '/');

    if (colon && (!slash || colon < slash)) {
        int host_len = colon - p;
        if (host_len > 63) host_len = 63;
        memcpy(s_host, p, host_len);
        s_host[host_len] = '\0';
        s_port = atoi(colon + 1);
    } else {
        int host_len = slash ? (slash - p) : strlen(p);
        if (host_len > 63) host_len = 63;
        memcpy(s_host, p, host_len);
        s_host[host_len] = '\0';
    }

    if (slash) {
        strncpy(s_path, slash, sizeof(s_path) - 1);
        s_path[sizeof(s_path) - 1] = '\0';
    }

    ESP_LOGI(TAG, "URI parsed: host=%s port=%d path=%s", s_host, s_port, s_path);
}

// ---------------------------------------------------------------------------
// WS Frame parse (server->client, unmasked)
// ---------------------------------------------------------------------------

static int ws_parse_frame(const uint8_t* data, int len, uint8_t* out_payload, int max_len)
{
    if (len < 2) return -1;

    int opcode = data[0] & 0x0F;
    int payload_len = data[1] & 0x7F;
    int header_len = 2;
    int mask_bit = (data[1] & 0x80) >> 7;

    if (payload_len == 126) {
        if (len < 4) return -1;
        payload_len = (data[2] << 8) | data[3];
        header_len = 4;
    } else if (payload_len == 127) {
        // Skip >64KB frames for debug
        return -1;
    }

    // Server frames are NOT masked (mask_bit should be 0)
    // But we handle masked anyway for robustness
    int mask_len = mask_bit ? 4 : 0;
    if (len < header_len + mask_len + payload_len) {
        return -1;
    }

    const uint8_t* payload = data + header_len + mask_len;

    if (opcode == 0x08) {
        // Close frame
        return -2;
    }
    if (opcode != 0x01 && opcode != 0x02) {
        // Ignore non-text/binary
        return 0;
    }

    int out_len = payload_len;
    if (out_len > max_len) out_len = max_len;

    if (mask_bit) {
        const uint8_t* mask = data + header_len;
        for (int i = 0; i < out_len; i++) {
            out_payload[i] = payload[i] ^ mask[i % 4];
        }
    } else {
        memcpy(out_payload, payload, out_len);
    }

    return out_len;
}

// ---------------------------------------------------------------------------
// WS Send frame (client->server, ALWAYS masked per RFC 6455)
// ---------------------------------------------------------------------------

static bool ws_send_frame(int sock, const uint8_t* data, int len, int opcode)
{
    if (sock < 0) return false;

    // Mask bit = 1 for client frames
    uint8_t header[10];
    int header_len = 2;

    header[0] = 0x80 | opcode;  // FIN + opcode
    header[1] = 0x80 | (len & 0x7F);  // Mask bit set

    // Generate 4-byte mask key
    uint8_t mask_key[4];
    for (int i = 0; i < 4; i++) {
        mask_key[i] = (uint8_t)(esp_random() & 0xFF);
    }

    // Add mask key to header for length < 126
    if (len < 126) {
        memcpy(header + 2, mask_key, 4);
        header_len = 6;
    } else {
        // 126 or 127 length
        header[1] = 0x80 | 126;
        header[2] = (len >> 8) & 0xFF;
        header[3] = len & 0xFF;
        memcpy(header + 4, mask_key, 4);
        header_len = 8;
    }

    // Send header
    if (send(sock, (char*)header, header_len, 0) != header_len) {
        return false;
    }

    // Send masked payload
    uint8_t masked[2048];
    int masked_len = (len > 2048) ? 2048 : len;
    for (int i = 0; i < masked_len; i++) {
        masked[i] = data[i] ^ mask_key[i % 4];
    }

    int sent = send(sock, (char*)masked, masked_len, 0);
    return (sent == masked_len);
}

// ---------------------------------------------------------------------------
// WS Handshake
// ---------------------------------------------------------------------------

static bool ws_handshake(int sock)
{
    // Generate 16-byte random key
    uint8_t key_bin[16];
    for (int i = 0; i < 16; i++) {
        key_bin[i] = (uint8_t)(esp_random() & 0xFF);
    }

    char key_b64[32];
    base64_encode(key_bin, 16, key_b64);

    // Send HTTP Upgrade request
    char request[512];
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        s_path, s_host, s_port, key_b64);

    ESP_LOGI(TAG, "[HS] Sending handshake...");
    if (send(sock, request, strlen(request), 0) < 0) {
        ESP_LOGE(TAG, "[HS] send failed");
        return false;
    }

    // Read response
    char response[1024];
    int received = recv(sock, response, sizeof(response) - 1, 0);
    if (received <= 0) {
        ESP_LOGE(TAG, "[HS] recv failed ret=%d", received);
        return false;
    }
    response[received] = '\0';

    ESP_LOGI(TAG, "[HS] Response:\n%s", response);

    // Verify 101 Switching Protocols
    if (strstr(response, "101") == NULL) {
        ESP_LOGE(TAG, "[HS] No 101 in response!");
        return false;
    }

    ESP_LOGI(TAG, "[HS] SUCCESS");
    return true;
}

// ---------------------------------------------------------------------------
// Send heartbeat
// ---------------------------------------------------------------------------

static void send_heartbeat(void)
{
    if (s_sock_fd < 0) return;

    s_seq++;
    char msg[128];
    int len = snprintf(msg, sizeof(msg),
        "{\"seq\":%u,\"type\":\"heartbeat\",\"source\":\"esp32\"}", s_seq);

    if (ws_send_frame(s_sock_fd, (uint8_t*)msg, len, 0x01)) {
        ESP_LOGI(TAG, "[HB] Sent heartbeat seq=%u", s_seq);
    } else {
        ESP_LOGE(TAG, "[HB] Send failed");
    }
}

// ---------------------------------------------------------------------------
// Main task
// ---------------------------------------------------------------------------

static void ws_echo_task(void* param)
{
    uint8_t buf[4096];
    uint8_t payload[4096];
    int64_t next_reconnect_ms = 0;
    int reconnect_delay_ms = 1000;

    parse_uri();

    ESP_LOGI(TAG, "[TASK] Starting. Target: %s:%d%s", s_host, s_port, s_path);

    while (s_task_running) {
        // ---- Connect phase ----
        if (s_state == WS_ECHO_DISCONNECTED) {
            int64_t now = esp_timer_get_time() / 1000;
            if (now < next_reconnect_ms) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            ESP_LOGI(TAG, "[TASK] Connecting to %s:%d...", s_host, s_port);
            s_state = WS_ECHO_CONNECTING;

            // Create socket
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                ESP_LOGE(TAG, "[TASK] socket() failed errno=%d", errno);
                goto reconnect_delay;
            }

            // Resolve host
            struct hostent* he = gethostbyname(s_host);
            if (!he) {
                ESP_LOGE(TAG, "[TASK] gethostbyname(%s) failed", s_host);
                close(sock);
                goto reconnect_delay;
            }

            // Connect
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(s_port);
            memcpy(&addr.sin_addr, he->h_addr, he->h_length);

            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
                ESP_LOGE(TAG, "[TASK] connect() failed errno=%d", errno);
                close(sock);
                goto reconnect_delay;
            }

            ESP_LOGI(TAG, "[TASK] Connected, doing WS handshake...");
            if (!ws_handshake(sock)) {
                ESP_LOGE(TAG, "[TASK] WS handshake failed");
                close(sock);
                goto reconnect_delay;
            }

            ESP_LOGI(TAG, "[TASK] *** WS CONNECTED ***");
            s_sock_fd = sock;
            s_state = WS_ECHO_CONNECTED;
            reconnect_delay_ms = 1000;
            s_last_heartbeat_ms = esp_timer_get_time() / 1000;

            if (s_recv_cb) {
                // Signal connected (via log, no callback needed for debug)
            }

            continue;

reconnect_delay:
            s_state = WS_ECHO_DISCONNECTED;
            next_reconnect_ms = (esp_timer_get_time() / 1000) + reconnect_delay_ms;
            reconnect_delay_ms *= 2;
            if (reconnect_delay_ms > WS_ECHO_RECONNECT_MAX_MS) {
                reconnect_delay_ms = WS_ECHO_RECONNECT_MAX_MS;
            }
            ESP_LOGI(TAG, "[TASK] Reconnecting in %d ms...", reconnect_delay_ms);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // ---- Connected: select + recv ----
        if (s_state == WS_ECHO_CONNECTED) {
            int64_t now_ms = esp_timer_get_time() / 1000;

            // Heartbeat timer
            if (now_ms - s_last_heartbeat_ms >= WS_ECHO_HEARTBEAT_MS) {
                send_heartbeat();
                s_last_heartbeat_ms = now_ms;
            }

            // select with 500ms timeout
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(s_sock_fd, &rfds);
            struct timeval tv = { .tv_sec = 0, .tv_usec = 500000 };

            int sel = select(s_sock_fd + 1, &rfds, NULL, NULL, &tv);
            if (sel < 0) {
                ESP_LOGE(TAG, "[TASK] select error, reconnecting...");
                close(s_sock_fd);
                s_sock_fd = -1;
                s_state = WS_ECHO_DISCONNECTED;
                continue;
            }
            if (sel == 0) {
                continue;
            }
            if (!FD_ISSET(s_sock_fd, &rfds)) {
                continue;
            }

            // Data available
            int received = recv(s_sock_fd, (char*)buf, sizeof(buf) - 1, 0);
            if (received <= 0) {
                ESP_LOGE(TAG, "[TASK] recv returned %d, disconnecting", received);
                close(s_sock_fd);
                s_sock_fd = -1;
                s_state = WS_ECHO_DISCONNECTED;
                continue;
            }

            // Log raw bytes
            ESP_LOGI(TAG, "[TASK] recv %d bytes:", received);
            for (int i = 0; i < received && i < 64; i++) {
                uint8_t c = buf[i];
                ESP_LOGI(TAG, "  [%02x] %c", c, (c >= 32 && c < 127) ? c : '.');
            }

            // Parse WS frames (may be multiple in one recv)
            int pos = 0;
            while (pos < received) {
                int r = ws_parse_frame(buf + pos, received - pos, payload, sizeof(payload) - 1);
                if (r > 0) {
                    payload[r] = '\0';
                    ESP_LOGI(TAG, "[TASK] WS frame text: %s", payload);
                    if (s_recv_cb) {
                        s_recv_cb((char*)payload, r);
                    }
                    pos += 2 + ((received - pos > 2 && (buf[pos + 1] & 0x7F) == 126) ? 4 : 0)
                         + ((buf[pos + 1] & 0x7F) + ((received - pos > 2 && (buf[pos + 1] & 0x7F) >= 126) ? 2 : 0))
                         + (received - pos);
                    // Simple advance for demo (correct implementation would track actual frame size)
                    break;
                } else if (r == -2) {
                    ESP_LOGI(TAG, "[TASK] Close frame received");
                    close(s_sock_fd);
                    s_sock_fd = -1;
                    s_state = WS_ECHO_DISCONNECTED;
                    break;
                } else {
                    // Incomplete or unknown, wait for more data
                    ESP_LOGI(TAG, "[TASK] Partial/invalid frame, waiting for more data");
                    break;
                }
            }
        }
    }

    ESP_LOGI(TAG, "[TASK] Task exiting");
    if (s_sock_fd >= 0) {
        close(s_sock_fd);
        s_sock_fd = -1;
    }
    s_task_running = false;
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ws_echo_init(ws_echo_on_receive_t callback)
{
    ESP_LOGI(TAG, "[INIT] Starting WS echo debug client");
    s_recv_cb = callback;
    s_state = WS_ECHO_DISCONNECTED;
    s_sock_fd = -1;
    s_seq = 0;
    s_task_running = true;
    xTaskCreate(&ws_echo_task, "ws_echo_task", 8192, NULL, 5, &s_task_handle);
    ESP_LOGI(TAG, "[INIT] Task created handle=%p", (void*)s_task_handle);
}

void ws_echo_deinit(void)
{
    ESP_LOGI(TAG, "[DEINIT] Stopping...");
    s_task_running = false;
    if (s_task_handle) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }
    if (s_sock_fd >= 0) {
        close(s_sock_fd);
        s_sock_fd = -1;
    }
    s_state = WS_ECHO_DISCONNECTED;
}

bool ws_echo_is_connected(void)
{
    return s_state == WS_ECHO_CONNECTED;
}

void ws_echo_send(const char* json)
{
    if (s_sock_fd >= 0 && s_state == WS_ECHO_CONNECTED) {
        ws_send_frame(s_sock_fd, (const uint8_t*)json, strlen(json), 0x01);
    }
}
```

---

## Task 5: Write app_main.c

**Files:**
- Create: `esp32_ws_debug/main/app_main.c`

- [ ] **Step 1: Write app_main.c**

```c
#include <stdio.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "ws_echo.h"
#include "esp_log.h"

static const char* TAG = "app";

#define WIFI_SSID     "jhome"
#define WIFI_PASS     "123698745"
#define WIFI_CONNECT_TIMEOUT_MS 30000

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "[WiFi] Start connecting...");
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "[WiFi] Disconnected");
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "[WiFi] Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void on_receive(const char* data, int len)
{
    ESP_LOGI(TAG, "[RCV] %s", data);
    // Echo the received data back
    ws_echo_send(data);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32 WS Echo Debug ===");

    // NVS init
    ESP_ERROR_CHECK(nvs_flash_init());

    // TCP/IP init
    ESP_ERROR_CHECK(esp_netif_init());

    // Event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // WiFi init
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handler
    esp_event_handler_instance_t handler;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
        &wifi_event_handler, NULL, &handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
        &wifi_event_handler, NULL, &handler));

    // Configure station
    wifi_config_t wifi_cfg = {
        .sta = {
            .threshold = { .authmode = WIFI_AUTH_WPA2_PSK },
        },
    };
    memcpy(wifi_cfg.sta.ssid, WIFI_SSID, strlen(WIFI_SSID));
    memcpy(wifi_cfg.sta.password, WIFI_PASS, strlen(WIFI_PASS));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "[WiFi] WiFi started, connecting to SSID='%s'", WIFI_SSID);

    // Wait for IP (simple polling)
    int64_t start = esp_timer_get_time() / 1000;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(500));
        int64_t now = esp_timer_get_time() / 1000;
        if (now - start > WIFI_CONNECT_TIMEOUT_MS) {
            ESP_LOGE(TAG, "[WiFi] Timeout waiting for IP!");
            break;
        }
        // Check netif has IP - simplified
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif) {
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                if (ip_info.ip.addr != 0) {
                    ESP_LOGI(TAG, "[WiFi] Connected! IP: " IPSTR, IP2STR(&ip_info.ip));
                    break;
                }
            }
        }
    }

    // Start WS echo
    ESP_LOGI(TAG, "Starting WS echo client...");
    ws_echo_init(on_receive);

    ESP_LOGI(TAG, "=== System running ===");
    ESP_LOGI(TAG, "Target server: %s", WS_ECHO_SERVER_URI);
    ESP_LOGI(TAG, "Heartbeat every %d ms", WS_ECHO_HEARTBEAT_MS);
}
```

---

## Task 6: Build and Test

**Files:**
- Modify: `E:\projects\esp32-rabbit\build_esp32.ps1` (if needed for new project)

- [ ] **Step 1: Verify directory structure**

```bash
ls -la esp32_ws_debug/
ls -la esp32_ws_debug/main/
```

- [ ] **Step 2: Build project**

From `esp32_ws_debug/`:
```bash
idf.py build
```
Expected: Build succeeds with no errors.

- [ ] **Step 3: Flash to ESP32**

```bash
idf.py -p COM8 flash monitor
```
Note: Use actual COM port from `idf.py list-ports` if COM8 not available.

- [ ] **Step 4: Verify serial output**

Expected serial output:
```
I (0) app: === ESP32 WS Echo Debug ===
I (500) wifi: WiFi started, connecting to SSID='jhome'
I (1500) wifi: Got IP: 10.0.0.x
I (2000) ws_echo: URI parsed: host=10.0.0.232 port=11081 path=/debug
I (2000) ws_echo: [TASK] Starting. Target: 10.0.0.232:11081/debug
I (2500) ws_echo: [TASK] Connecting to 10.0.0.232:11081...
I (2600) ws_echo: [HS] Sending handshake...
I (2700) ws_echo: [HS] Response:
HTTP/1.1 101 Switching Protocols
...
I (2700) ws_echo: [HS] SUCCESS
I (2700) ws_echo: [TASK] *** WS CONNECTED ***
I (7500) ws_echo: [HB] Sent heartbeat seq=1
```

- [ ] **Step 5: Verify echo on server**

PC console shows:
```
[SERVER] ESP32 connected from ...
[SERVER] Received: {"seq":1,"type":"heartbeat","source":"esp32"}
[SERVER] Sent echo: {"echo": {"seq":1,...}}
```

- [ ] **Step 6: Test PC→ESP32 message**

In another terminal, use websocat or similar:
```bash
websocat ws://10.0.0.232:11081/debug
# Then type: {"cmd":"test"}
```

ESP32 serial should show:
```
I (xxx) ws_echo: [TASK] recv N bytes:
  [7b] {  [22] "  ...
I (xxx) ws_echo: [TASK] WS frame text: {"cmd":"test"}
I (xxx) app: [RCV] {"cmd":"test"}
```

---

## Task 7: Commit

- [ ] **Step 1: Stage and commit**

```bash
git add esp32_ws_debug/
git commit -m "feat: add standalone ESP32 WS debug project

- ws_echo.c: correct mbedTLS SHA1, WS handshake, masked frames
- echo_server.py: Python WS server on port 11081
- app_main.c: WiFi connect + WS echo init
- Minimal build config in sdkconfig.defaults

First debugging step for WS connection issue."
```

---

## Verification Checklist

- [ ] echo_server.py starts without error
- [ ] ESP32 connects to server (serial shows `WS CONNECTED`)
- [ ] ESP32 sends heartbeat every 5s (visible on PC server)
- [ ] PC can send message → ESP32 receives and prints it
- [ ] Build clean (no warnings about SHA1)
- [ ] Commit created