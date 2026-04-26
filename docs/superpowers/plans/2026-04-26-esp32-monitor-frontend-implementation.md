# ESP32 监控前端实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 纯前端 HTML/CSS/JS 实现 ESP32 摄像头监控与舵机控制

**Architecture:** 单页应用，直接调用 ESP32 HTTP API（MJPEG 流 + 控制端点），无后端依赖。三阶段部署：本地浏览器 → Docker → 远程 Nginx

**Tech Stack:** HTML5, CSS3, Vanilla JS, Fetch API

**Design System:**
- Color: Background #0d1117, Surface #161b22, Border #30363d, Primary #58a6ff, Success #3fb950, Danger #f85149, Text #e6edf3, Muted #8b949e
- Typography: 'JetBrains Mono' for headings (tech feel), system-ui for body
- Spacing: 8px base unit (multiples: 8, 16, 24, 32, 48)
- Border-radius: 8px cards, 6px buttons, 4px small elements
- Motion: 150ms ease-out transitions, hover scale(1.02) + shadow lift

---

## Task 1: 创建设计系统 index.html

**Files:**
- Create: `esp32-monitor/index.html`

- [ ] **Step 1: 创建 index.html**

```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Monitor</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;500;600;700&display=swap" rel="stylesheet">
    <style>
        /* ============================================
           DESIGN TOKENS
           ============================================ */
        :root {
            /* Colors */
            --bg: #0d1117;
            --surface: #161b22;
            --border: #30363d;
            --primary: #58a6ff;
            --success: #3fb950;
            --danger: #f85149;
            --text: #e6edf3;
            --muted: #8b949e;

            /* Spacing */
            --sp-1: 8px;
            --sp-2: 16px;
            --sp-3: 24px;
            --sp-4: 32px;
            --sp-5: 48px;

            /* Radius */
            --r-lg: 8px;
            --r-md: 6px;
            --r-sm: 4px;

            /* Shadow */
            --shadow-1: 0 1px 3px rgba(0,0,0,0.3);
            --shadow-2: 0 4px 12px rgba(0,0,0,0.4);
        }

        /* ============================================
           RESET & BASE
           ============================================ */
        *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

        body {
            font-family: system-ui, -apple-system, sans-serif;
            background: var(--bg);
            color: var(--text);
            min-height: 100vh;
            padding: var(--sp-3);
            line-height: 1.5;
        }

        /* ============================================
           LAYOUT
           ============================================ */
        .container {
            max-width: 800px;
            margin: 0 auto;
        }

        /* ============================================
           HEADER
           ============================================ */
        .header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: var(--sp-2) var(--sp-3);
            background: var(--surface);
            border: 1px solid var(--border);
            border-radius: var(--r-lg);
            margin-bottom: var(--sp-3);
        }

        .header-title {
            font-family: 'JetBrains Mono', monospace;
            font-size: 20px;
            font-weight: 600;
            color: var(--primary);
            display: flex;
            align-items: center;
            gap: var(--sp-2);
        }

        .status-dot {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background: var(--muted);
            transition: background 150ms ease-out;
        }

        .status-dot.connected { background: var(--success); }
        .status-dot.error { background: var(--danger); }

        /* ============================================
           SECTION CARD
           ============================================ */
        .card {
            background: var(--surface);
            border: 1px solid var(--border);
            border-radius: var(--r-lg);
            padding: var(--sp-3);
            margin-bottom: var(--sp-3);
            transition: box-shadow 150ms ease-out;
        }

        .card:hover {
            box-shadow: var(--shadow-2);
        }

        .card-label {
            font-family: 'JetBrains Mono', monospace;
            font-size: 12px;
            font-weight: 500;
            color: var(--muted);
            text-transform: uppercase;
            letter-spacing: 0.05em;
            margin-bottom: var(--sp-2);
        }

        /* ============================================
           CAMERA SECTION
           ============================================ */
        .camera-controls {
            display: flex;
            gap: var(--sp-2);
        }

        .btn {
            flex: 1;
            padding: 12px 24px;
            border: none;
            border-radius: var(--r-md);
            font-family: 'JetBrains Mono', monospace;
            font-size: 14px;
            font-weight: 500;
            cursor: pointer;
            transition: all 150ms ease-out;
        }

        .btn:hover:not(:disabled) {
            transform: scale(1.02);
        }

        .btn:active:not(:disabled) {
            transform: scale(0.98);
        }

        .btn:disabled {
            opacity: 0.4;
            cursor: not-allowed;
        }

        .btn-on {
            background: var(--success);
            color: #0d1117;
        }

        .btn-on:hover:not(:disabled) {
            box-shadow: 0 0 20px rgba(63, 185, 80, 0.3);
        }

        .btn-off {
            background: var(--danger);
            color: #fff;
        }

        .btn-off:hover:not(:disabled) {
            box-shadow: 0 0 20px rgba(248, 81, 73, 0.3);
        }

        /* ============================================
           VIDEO SECTION
           ============================================ */
        .video-wrap {
            position: relative;
            background: #000;
            border-radius: var(--r-md);
            overflow: hidden;
            aspect-ratio: 4/3;
        }

        .video-wrap img {
            width: 100%;
            height: 100%;
            object-fit: cover;
            display: block;
        }

        .video-placeholder {
            position: absolute;
            inset: 0;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            background: var(--bg);
            color: var(--muted);
            font-family: 'JetBrains Mono', monospace;
            gap: var(--sp-2);
        }

        .video-placeholder .icon {
            font-size: 48px;
            opacity: 0.5;
        }

        .video-placeholder.hidden { display: none; }

        /* ============================================
           SERVO SECTION
           ============================================ */
        .servo-display {
            display: flex;
            align-items: baseline;
            gap: var(--sp-1);
            margin-bottom: var(--sp-2);
        }

        .servo-value {
            font-family: 'JetBrains Mono', monospace;
            font-size: 48px;
            font-weight: 700;
            color: var(--primary);
            line-height: 1;
        }

        .servo-unit {
            font-family: 'JetBrains Mono', monospace;
            font-size: 20px;
            color: var(--muted);
        }

        .slider-wrap {
            position: relative;
        }

        .slider {
            -webkit-appearance: none;
            width: 100%;
            height: 8px;
            background: var(--border);
            border-radius: 4px;
            outline: none;
            cursor: pointer;
        }

        .slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 24px;
            height: 24px;
            background: var(--primary);
            border-radius: 50%;
            cursor: pointer;
            box-shadow: 0 0 10px rgba(88, 166, 255, 0.4);
            transition: box-shadow 150ms ease-out;
        }

        .slider::-webkit-slider-thumb:hover {
            box-shadow: 0 0 20px rgba(88, 166, 255, 0.6);
        }

        .slider::-moz-range-thumb {
            width: 24px;
            height: 24px;
            background: var(--primary);
            border: none;
            border-radius: 50%;
            cursor: pointer;
        }

        /* ============================================
           CONNECTION STATUS
           ============================================ */
        .footer {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: var(--sp-2) 0;
            border-top: 1px solid var(--border);
            margin-top: var(--sp-2);
        }

        .footer-info {
            font-family: 'JetBrains Mono', monospace;
            font-size: 12px;
            color: var(--muted);
        }

        .connection-status {
            display: flex;
            align-items: center;
            gap: var(--sp-1);
            font-family: 'JetBrains Mono', monospace;
            font-size: 12px;
            font-weight: 500;
        }

        .connection-status.ok { color: var(--success); }
        .connection-status.error { color: var(--danger); }
        .connection-status.pending { color: var(--muted); }
    </style>
</head>
<body>
    <div class="container">
        <!-- Header -->
        <header class="header">
            <div class="header-title">
                <span class="status-dot" id="status-dot"></span>
                ESP32 Monitor
            </div>
        </header>

        <!-- Camera Section -->
        <section class="card" id="camera-card">
            <div class="card-label">Camera</div>
            <div class="camera-controls">
                <button class="btn btn-on" id="btn-camera-on">ON</button>
                <button class="btn btn-off" id="btn-camera-off">OFF</button>
            </div>
        </section>

        <!-- Video Stream Section -->
        <section class="card" id="video-card">
            <div class="card-label">Stream</div>
            <div class="video-wrap">
                <img id="video-stream" src="" alt="Video stream">
                <div class="video-placeholder hidden" id="video-placeholder">
                    <div class="icon">[ ]</div>
                    <div>Camera Off</div>
                </div>
            </div>
        </section>

        <!-- Servo Control Section -->
        <section class="card" id="servo-card">
            <div class="card-label">Servo</div>
            <div class="servo-display">
                <span class="servo-value" id="servo-value">90</span>
                <span class="servo-unit">°</span>
            </div>
            <div class="slider-wrap">
                <input type="range" class="slider" id="servo-slider" min="0" max="180" value="90">
            </div>
        </section>

        <!-- Footer -->
        <footer class="footer">
            <div class="footer-info">10.0.0.110:8080 / 8081</div>
            <div class="connection-status pending" id="connection-status">
                <span>Connecting...</span>
            </div>
        </footer>
    </div>

    <script src="app.js"></script>
</body>
</html>
```

---

## Task 2: 创建 app.js

**Files:**
- Create: `esp32-monitor/app.js`

- [ ] **Step 1: 创建 app.js**

```javascript
/**
 * ESP32 Monitor - Frontend Logic
 * Direct ESP32 HTTP API calls, no backend
 */

(function() {
    'use strict';

    // ========== CONFIG ==========
    const ESP32_IP = '10.0.0.110';
    const CONTROL_PORT = 8080;
    const STREAM_PORT = 8081;
    const CONTROL_URL = `http://${ESP32_IP}:${CONTROL_PORT}/control`;
    const STREAM_URL = `http://${ESP32_IP}:${STREAM_PORT}/stream`;
    const POLL_INTERVAL = 5000;
    const REQUEST_TIMEOUT = 10000;

    // ========== STATE ==========
    let cameraOn = false;
    let servoAngle = 90;
    let pollTimer = null;

    // ========== DOM REFS ==========
    const dom = {
        statusDot: document.getElementById('status-dot'),
        btnCameraOn: document.getElementById('btn-camera-on'),
        btnCameraOff: document.getElementById('btn-camera-off'),
        videoStream: document.getElementById('video-stream'),
        videoPlaceholder: document.getElementById('video-placeholder'),
        servoSlider: document.getElementById('servo-slider'),
        servoValue: document.getElementById('servo-value'),
        connectionStatus: document.getElementById('connection-status'),
    };

    // ========== INIT ==========
    function init() {
        setupEventListeners();
        startPolling();
        syncVideoSrc();
        updateConnectionUI('pending', 'Connecting...');
    }

    // ========== EVENT LISTENERS ==========
    function setupEventListeners() {
        dom.btnCameraOn.addEventListener('click', () => setCamera(true));
        dom.btnCameraOff.addEventListener('click', () => setCamera(false));

        let debounceTimer = null;
        dom.servoSlider.addEventListener('input', (e) => {
            const val = parseInt(e.target.value, 10);
            dom.servoValue.textContent = val;
        });

        dom.servoSlider.addEventListener('change', (e) => {
            const val = parseInt(e.target.value, 10);
            sendServoAngle(val);
        });
    }

    // ========== API CALLS ==========
    async function setCamera(on) {
        try {
            const resp = await fetch(CONTROL_URL, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ action: on ? 'on' : 'off' }),
                signal: AbortSignal.timeout(REQUEST_TIMEOUT),
            });
            const data = await resp.json();
            if (data.status === 'ok') {
                cameraOn = on;
                updateVideoDisplay();
                updateCameraButtons();
                updateConnectionUI('ok', 'Connected');
            }
        } catch (err) {
            updateConnectionUI('error', 'Connection failed');
        }
    }

    async function sendServoAngle(angle) {
        try {
            const resp = await fetch(CONTROL_URL, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ servo: angle }),
                signal: AbortSignal.timeout(REQUEST_TIMEOUT),
            });
            const data = await resp.json();
            if (data.status === 'ok' && data.servo !== undefined) {
                servoAngle = data.servo;
                dom.servoSlider.value = servoAngle;
                dom.servoValue.textContent = servoAngle;
            }
        } catch (err) {
            updateConnectionUI('error', 'Connection failed');
        }
    }

    async function fetchStatus() {
        try {
            const resp = await fetch(CONTROL_URL, {
                signal: AbortSignal.timeout(REQUEST_TIMEOUT),
            });
            const data = await resp.json();
            if (data.status === 'ok') {
                cameraOn = data.camera === 'on';
                if (data.servo !== undefined) {
                    servoAngle = data.servo;
                    dom.servoSlider.value = servoAngle;
                    dom.servoValue.textContent = servoAngle;
                }
                updateVideoDisplay();
                updateCameraButtons();
                updateConnectionUI('ok', 'Connected');
            }
        } catch (err) {
            updateConnectionUI('error', 'Connection failed');
        }
    }

    // ========== UI UPDATES ==========
    function updateVideoDisplay() {
        if (cameraOn) {
            // Refresh stream with cache-bust
            dom.videoStream.src = `${STREAM_URL}?t=${Date.now()}`;
            dom.videoStream.style.display = 'block';
            dom.videoPlaceholder.classList.add('hidden');
        } else {
            dom.videoStream.src = '';
            dom.videoStream.style.display = 'none';
            dom.videoPlaceholder.classList.remove('hidden');
        }
    }

    function updateCameraButtons() {
        dom.btnCameraOn.disabled = cameraOn;
        dom.btnCameraOff.disabled = !cameraOn;
    }

    function updateConnectionUI(state, message) {
        dom.statusDot.className = 'status-dot';
        dom.connectionStatus.className = `connection-status ${state}`;

        if (state === 'ok') {
            dom.statusDot.classList.add('connected');
        } else if (state === 'error') {
            dom.statusDot.classList.add('error');
        }

        dom.connectionStatus.querySelector('span').textContent = message;
    }

    function syncVideoSrc() {
        // Initial video src sync
        dom.videoStream.src = STREAM_URL;
    }

    // ========== POLLING ==========
    function startPolling() {
        fetchStatus();
        pollTimer = setInterval(fetchStatus, POLL_INTERVAL);
    }

    // ========== START ==========
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }
})();
```

---

## Task 3: Docker 部署配置

**Files:**
- Create: `esp32-monitor/Dockerfile`
- Create: `esp32-monitor/nginx.conf`
- Create: `esp32-monitor/.dockerignore`
- Create: `esp32-monitor/docker-compose.yml`

- [ ] **Step 1: 创建 Dockerfile**

```dockerfile
FROM nginx:alpine
COPY . /usr/share/nginx/html
EXPOSE 80
CMD ["nginx", "-c", "/etc/nginx/nginx.conf"]
```

- [ ] **Step 2: 创建 nginx.conf**

```nginx
events {
    worker_connections 1024;
}

http {
    include /etc/nginx/mime.types;
    default_type application/octet-stream;

    server {
        listen 80;
        server_name localhost;
        root /usr/share/nginx/html;
        index index.html;

        location / {
            try_files $uri $uri/ =404;
        }

        location ~ /\. {
            deny all;
        }
    }
}
```

- [ ] **Step 3: 创建 .dockerignore**

```
*.md
.git
```

- [ ] **Step 4: 创建 docker-compose.yml**

```yaml
version: '3.8'
services:
  esp32-monitor:
    build: .
    ports:
      - "8080:80"
```

---

## Task 4: pytest ESP32 API 集成测试

**Files:**
- Create: `tests/conftest.py` (已存在，追加 ESP32 相关 fixtures)
- Create: `tests/test_esp32_api.py`

- [ ] **Step 1: 检查并更新 conftest.py**

```python
# Append to existing tests/conftest.py

def pytest_addoption(parser):
    parser.addoption("--esp32-ip", action="store", default="10.0.0.110",
                    help="ESP32 IP address")
    parser.addoption("--control-port", action="store", default="8080",
                    help="ESP32 control port")
    parser.addoption("--stream-port", action="store", default="8081",
                    help="ESP32 stream port")


@pytest.fixture
def esp32_ip(request):
    return request.config.getoption("--esp32-ip")


@pytest.fixture
def control_port(request):
    return request.config.getoption("--control-port")


@pytest.fixture
def stream_port(request):
    return request.config.getoption("--stream-port")


@pytest.fixture
def base_url(esp32_ip, control_port):
    return f"http://{esp32_ip}:{control_port}"


@pytest.fixture
def stream_url(esp32_ip, stream_port):
    return f"http://{esp32_ip}:{stream_port}"


@pytest.fixture
def session():
    import requests
    return requests.Session()
```

- [ ] **Step 2: 创建 test_esp32_api.py**

```python
"""
ESP32 控制 API 集成测试

需要 ESP32 固件运行在 10.0.0.110:8080

Usage:
    pytest tests/test_esp32_api.py -v
    pytest tests/test_esp32_api.py -v --esp32-ip 10.0.0.110
"""

import pytest


def test_control_get_status(base_url, session):
    """GET /control returns current status"""
    resp = session.get(f"{base_url}/control", timeout=10)
    assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"
    data = resp.json()
    assert data.get("status") == "ok", f"Expected status=ok, got {data}"


def test_control_post_camera_on(base_url, session):
    """POST /control with action=on"""
    resp = session.post(
        f"{base_url}/control",
        json={"action": "on"},
        headers={"Content-Type": "application/json"},
        timeout=10
    )
    assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"
    data = resp.json()
    assert data.get("camera") == "on", f"Expected camera=on, got {data}"


def test_control_post_camera_off(base_url, session):
    """POST /control with action=off"""
    resp = session.post(
        f"{base_url}/control",
        json={"action": "off"},
        headers={"Content-Type": "application/json"},
        timeout=10
    )
    assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"
    data = resp.json()
    assert data.get("camera") == "off", f"Expected camera=off, got {data}"


def test_control_post_servo(base_url, session):
    """POST /control with servo angle"""
    resp = session.post(
        f"{base_url}/control",
        json={"servo": 45},
        headers={"Content-Type": "application/json"},
        timeout=10
    )
    assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"
    data = resp.json()
    assert data.get("servo") == 45, f"Expected servo=45, got {data}"


def test_stream_endpoint(stream_url):
    """GET /stream returns MJPEG stream"""
    import requests
    resp = requests.get(f"{stream_url}/stream", timeout=10, stream=True)
    assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"
    assert "multipart" in resp.headers.get("Content-Type", ""), \
        f"Expected multipart content-type"
```

---

## Task 5: 提交所有文件

- [ ] **Step 1: 创建 esp32-monitor 目录并提交**

```bash
git add esp32-monitor/
git commit -m "feat: add ESP32 monitor frontend (HTML/CSS/JS)

Design system: dark theme, JetBrains Mono, 8px spacing
- index.html: 主页面，MJPEG 视频流 + 摄像头开关 + 舵机滑动条
- app.js: ESP32 API 调用逻辑，状态轮询
- Dockerfile + nginx.conf + docker-compose.yml: Docker 部署

Three phases: local browser → Docker → remote Nginx.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

- [ ] **Step 2: 提交测试文件**

```bash
git add tests/
git commit -m "test: add ESP32 API integration tests

pytest tests/test_esp32_api.py -v
Requires ESP32 firmware running at 10.0.0.110:8080.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

## Task 6: /qa 浏览器测试验证

**Files:**
- 测试目标: `esp32-monitor/index.html`（本地打开或 Docker 启动后）

- [ ] **Step 1: 本地打开前端验证**

```bash
# 方式A: 直接在浏览器打开
# file:///<project-path>/esp32-monitor/index.html

# 方式B: Docker 启动后访问
docker build -t esp32-monitor ./esp32-monitor
docker run -d -p 8080:80 esp32-monitor
# 浏览器打开 http://localhost:8080
```

- [ ] **Step 2: 运行 /qa 测试**

```bash
# 确保 ESP32 运行在 10.0.0.110:8080/8081
# 然后在浏览器中运行：
/qa http://localhost:8080
```

- [ ] **Step 3: /qa 验证项**

- [ ] 页面加载无 console errors
- [ ] 摄像头 ON/OFF 按钮可见且可点击
- [ ] 按钮点击后 UI 状态更新
- [ ] 舵机 slider 可拖动，角度值实时更新
- [ ] 视频流区域显示（摄像头开启时）
- [ ] 连接状态指示器正确显示

---

## 验证步骤

1. **本地测试（无 ESP32）**
   ```bash
   # 直接打开 esp32-monitor/index.html
   # 验证页面渲染和样式正常
   ```

2. **ESP32 API 测试**
   ```bash
   # ESP32 固件运行后
   pytest tests/test_esp32_api.py -v --esp32-ip 10.0.0.110
   ```

3. **/qa 浏览器测试**
   ```bash
   # Docker 启动
   docker build -t esp32-monitor ./esp32-monitor
   docker run -d -p 8080:80 esp32-monitor

   # /qa 验证
   /qa http://localhost:8080
   ```

4. **Docker 部署验证（三阶段）**
   ```bash
   # 本地 docker-compose 测试通过后
   # 推送代码 → 远程服务器
   # 远程运行 docker-compose 部署
   ```

---

## Spec 覆盖检查

- [x] 摄像头开启/关闭功能 → Task 1, 2
- [x] MJPEG 视频流显示 → Task 1, 2
- [x] 舵机角度滑动控制 → Task 1, 2
- [x] 状态轮询更新 → Task 2
- [x] 三阶段部署配置 → Task 3
- [x] ESP32 API pytest 测试 → Task 4
- [x] /qa 浏览器 UI 测试 → Task 6