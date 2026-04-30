# ESP32 Monitor WebSocket Server - Python Port

## Overview

Port Node.js WebSocket server to Python using aiohttp. Single process handles HTTP static files + WebSocket message routing.

## Architecture

```
ESP32 (WS Client) ──► Python aiohttp WS Server ◄── Browser (WS Client)
                     │
                     └─► HTTP :8080 (Static files)
```

## Ports
- HTTP: 8080
- WebSocket: 11080

## Dependencies
```
aiohttp>=3.9.0
```

## WebSocket Protocol

**frontend → ESP32:**
```json
{"action": "on"}
{"action": "off"}
{"action": "servo", "angle": 90}
```

**ESP32 → frontend:**
```json
{"status": "ok", "camera": "on", "servo": 90}
```

## Server Behavior
1. First connected client = ESP32
2. Second connected client = Frontend
3. ESP32 消息 → 转发 frontend
4. Frontend 命令 → 转发 ESP32
5. 心跳：无需 ping/pong，TCP keepalive 处理

## Files
- `esp32-monitor/server.py` — Python WS server
- `esp32-monitor/app.js` — Frontend WS client
- `esp32-monitor/index.html` — UI
