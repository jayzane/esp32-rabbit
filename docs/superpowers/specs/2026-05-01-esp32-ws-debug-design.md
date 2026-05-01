# ESP32 WS Debug Design

## Goal

Debug ESP32 WebSocket connection to PC server. Create standalone project that:
- Connects ESP32 to PC WebSocket server
- Sends periodic heartbeat messages
- Prints all received messages to serial
- Uses correct SHA1 (mbedTLS) instead of fake implementation

## Architecture

```
esp32_ws_debug/
├── CMakeLists.txt
├── sdkconfig.defaults
├── main/
│   ├── CMakeLists.txt
│   ├── ws_echo.h          # Header
│   ├── ws_echo.c          # WS connect, send, receive, print
│   └── app_main.c         # Entry, timer task
└── server/
    └── echo_server.py     # Simple echo server on port 11081
```

## WS Echo Module

`ws_echo.c`:
- Connect to `ws://10.0.0.232:11081/debug` on boot
- On connect: print `WS CONNECTED to <host>:<port>`
- Every 5s: send `{"seq":N,"type":"heartbeat","source":"esp32"}`
- On receive: print raw bytes + decoded string
- Reconnect on disconnect with exponential backoff (max 30s)

## Server

`server/echo_server.py`:
- `aiohttp.web.WebSocketResponse`
- Listen on `0.0.0.0:11081`
- Path `/debug`
- On message: echo back `{"echo":<original>}`
- Log all received messages

## Build Config

`sdkconfig.defaults`:
- `CONFIG_ESP_WIFI_ENABLED=y`
- `CONFIG_ESP_NETIF_ENABLED=y`
- `CONFIG_LWIP_ENABLED=y`
- Minimize other components for fast build

## Implementation Steps

1. Create directory structure
2. Write `CMakeLists.txt` + `sdkconfig.defaults`
3. Write `ws_echo.h` + `ws_echo.c` (mbedTLS SHA1, correct WS handshake)
4. Write `app_main.c` (WiFi connect + task spawn)
5. Write `server/echo_server.py`
6. Build and flash

## Testing Flow

1. PC: `python server/echo_server.py` → listening on 11081
2. ESP32: flash + monitor → connects, prints connected
3. PC server sends `{"cmd":"test"}` → ESP32 prints received
4. ESP32 heartbeat appears on PC server console
5. Once confirmed working, switch server to 11080 for full integration test