# ESP32 监控前端实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 纯前端 HTML/CSS/JS 实现 ESP32 摄像头监控与舵机控制

**Architecture:** 单页应用，直接调用 ESP32 HTTP API（MJPEG 流 + 控制端点），无后端依赖

**Tech Stack:** HTML5, CSS3, Vanilla JS, Fetch API

---

## Task 1: 创建 index.html

**Files:**
- Create: `esp32-monitor/index.html`

- [ ] **Step 1: 创建 index.html**

```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 监控控制台</title>
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <div class="container">
        <h1>ESP32 监控控制台</h1>

        <!-- 摄像头控制 -->
        <section class="camera-control">
            <h2>摄像头</h2>
            <div class="button-group">
                <button id="btn-camera-on" class="btn btn-on">开启</button>
                <button id="btn-camera-off" class="btn btn-off">关闭</button>
            </div>
        </section>

        <!-- 视频流 -->
        <section class="video-section">
            <h2>视频流</h2>
            <div class="video-container" id="video-container">
                <img id="video-stream" src="" alt="视频流">
                <div id="video-placeholder" class="placeholder hidden">
                    摄像头已关闭
                </div>
            </div>
        </section>

        <!-- 舵机控制 -->
        <section class="servo-control">
            <h2>舵机控制</h2>
            <div class="slider-container">
                <label for="servo-slider">角度:</label>
                <input type="range" id="servo-slider" min="0" max="180" value="90">
                <span id="servo-value">90</span>°
            </div>
            <p>当前角度: <span id="current-angle">90</span>°</p>
        </section>

        <!-- 状态显示 -->
        <section class="status-section">
            <p>连接状态: <span id="connection-status">连接中...</span></p>
        </section>
    </div>

    <script src="app.js"></script>
</body>
</html>
```

---

## Task 2: 创建 style.css

**Files:**
- Create: `esp32-monitor/style.css`

- [ ] **Step 1: 创建 style.css**

```css
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: #1a1a2e;
    color: #eee;
    min-height: 100vh;
    padding: 20px;
}

.container {
    max-width: 800px;
    margin: 0 auto;
}

h1 {
    text-align: center;
    margin-bottom: 30px;
    color: #00d4ff;
}

h2 {
    margin-bottom: 15px;
    color: #00d4ff;
}

section {
    background: #16213e;
    border-radius: 10px;
    padding: 20px;
    margin-bottom: 20px;
}

/* 按钮样式 */
.button-group {
    display: flex;
    gap: 10px;
}

.btn {
    padding: 10px 30px;
    border: none;
    border-radius: 5px;
    font-size: 16px;
    cursor: pointer;
    transition: background 0.3s;
}

.btn-on {
    background: #00d4ff;
    color: #1a1a2e;
}

.btn-on:hover {
    background: #00a8cc;
}

.btn-off {
    background: #ff6b6b;
    color: #fff;
}

.btn-off:hover {
    background: #ee5a5a;
}

.btn:disabled {
    opacity: 0.5;
    cursor: not-allowed;
}

/* 视频流 */
.video-section {
    text-align: center;
}

.video-container {
    position: relative;
    background: #000;
    border-radius: 8px;
    overflow: hidden;
}

#video-stream {
    width: 100%;
    max-width: 640px;
    display: block;
    margin: 0 auto;
}

.placeholder {
    position: absolute;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    color: #666;
    font-size: 18px;
}

.hidden {
    display: none;
}

/* 舵机控制 */
.slider-container {
    display: flex;
    align-items: center;
    gap: 15px;
    margin-bottom: 15px;
}

#servo-slider {
    flex: 1;
    height: 8px;
    -webkit-appearance: none;
    background: #0f3460;
    border-radius: 4px;
    outline: none;
}

#servo-slider::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 20px;
    height: 20px;
    background: #00d4ff;
    border-radius: 50%;
    cursor: pointer;
}

#servo-value {
    font-size: 20px;
    font-weight: bold;
    color: #00d4ff;
    min-width: 50px;
}

/* 状态 */
.status-section p {
    font-size: 14px;
}

#connection-status {
    font-weight: bold;
}

.status-ok {
    color: #00ff88;
}

.status-error {
    color: #ff6b6b;
}
```

---

## Task 3: 创建 app.js

**Files:**
- Create: `esp32-monitor/app.js`

- [ ] **Step 1: 创建 app.js**

```javascript
// ESP32 配置
const ESP32_IP = '10.0.0.110';
const CONTROL_PORT = 8080;
const STREAM_PORT = 8081;
const CONTROL_URL = `http://${ESP32_IP}:${CONTROL_PORT}/control`;
const STREAM_URL = `http://${ESP32_IP}:${STREAM_PORT}/stream`;
const POLL_INTERVAL = 5000;
const REQUEST_TIMEOUT = 10000;

// DOM 元素
const btnCameraOn = document.getElementById('btn-camera-on');
const btnCameraOff = document.getElementById('btn-camera-off');
const videoStream = document.getElementById('video-stream');
const videoPlaceholder = document.getElementById('video-placeholder');
const servoSlider = document.getElementById('servo-slider');
const servoValue = document.getElementById('servo-value');
const currentAngle = document.getElementById('current-angle');
const connectionStatus = document.getElementById('connection-status');

// 状态
let cameraOn = false;
let currentServoAngle = 90;
let pollTimer = null;

// 初始化
function init() {
    videoStream.src = STREAM_URL;
    setupEventListeners();
    startPolling();
    updateConnectionStatus('连接中...');
}

// 事件监听
function setupEventListeners() {
    btnCameraOn.addEventListener('click', () => setCamera(true));
    btnCameraOff.addEventListener('click', () => setCamera(false));

    let debounceTimer;
    servoSlider.addEventListener('input', (e) => {
        const angle = parseInt(e.target.value);
        servoValue.textContent = angle;
    });
    servoSlider.addEventListener('change', (e) => {
        const angle = parseInt(e.target.value);
        setServoAngle(angle);
    });
}

// 设置摄像头
async function setCamera(on) {
    try {
        const resp = await fetch(CONTROL_URL, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({action: on ? 'on' : 'off'}),
            signal: AbortSignal.timeout(REQUEST_TIMEOUT)
        });
        const data = await resp.json();
        if (data.status === 'ok') {
            cameraOn = on;
            updateVideoDisplay();
            updateButtonStates();
        }
    } catch (err) {
        updateConnectionStatus('连接失败', true);
    }
}

// 设置舵机角度
async function setServoAngle(angle) {
    try {
        const resp = await fetch(CONTROL_URL, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({servo: angle}),
            signal: AbortSignal.timeout(REQUEST_TIMEOUT)
        });
        const data = await resp.json();
        if (data.status === 'ok' && data.servo !== undefined) {
            currentServoAngle = data.servo;
            currentAngle.textContent = currentServoAngle;
            servoSlider.value = currentServoAngle;
            servoValue.textContent = currentServoAngle;
        }
    } catch (err) {
        updateConnectionStatus('连接失败', true);
    }
}

// 获取状态
async function fetchStatus() {
    try {
        const resp = await fetch(CONTROL_URL, {
            signal: AbortSignal.timeout(REQUEST_TIMEOUT)
        });
        const data = await resp.json();
        if (data.status === 'ok') {
            cameraOn = data.camera === 'on';
            if (data.servo !== undefined) {
                currentServoAngle = data.servo;
                servoSlider.value = currentServoAngle;
                servoValue.textContent = currentServoAngle;
                currentAngle.textContent = currentServoAngle;
            }
            updateVideoDisplay();
            updateButtonStates();
            updateConnectionStatus('已连接', false);
        }
    } catch (err) {
        updateConnectionStatus('连接失败', true);
    }
}

// 更新视频显示
function updateVideoDisplay() {
    if (cameraOn) {
        videoStream.src = STREAM_URL + '?t=' + Date.now();
        videoStream.style.display = 'block';
        videoPlaceholder.classList.add('hidden');
    } else {
        videoStream.style.display = 'none';
        videoPlaceholder.classList.remove('hidden');
    }
}

// 更新按钮状态
function updateButtonStates() {
    btnCameraOn.disabled = cameraOn;
    btnCameraOff.disabled = !cameraOn;
}

// 更新连接状态
function updateConnectionStatus(msg, isError = false) {
    connectionStatus.textContent = msg;
    connectionStatus.className = isError ? 'status-error' : 'status-ok';
}

// 轮询状态
function startPolling() {
    fetchStatus();
    pollTimer = setInterval(fetchStatus, POLL_INTERVAL);
}

// 启动
init();
```

---

## Task 4: pytest 集成测试

**Files:**
- Create: `tests/test_esp32_monitor_frontend.py`
- Config: `pytest.ini` 或 `pyproject.toml`

- [ ] **Step 1: 创建 pytest 测试文件**

```python
"""
ESP32 监控前端 pytest 集成测试

测试前端静态文件结构和配置正确性。
需要 ESP32 固件运行在 10.0.0.110:8080

Usage:
    pytest tests/test_esp32_monitor_frontend.py -v
    pytest tests/test_esp32_monitor_frontend.py -v --esp32-ip 10.0.0.110
"""

import pytest
import json
import time


def test_index_html_exists():
    """验证 index.html 存在且包含必需元素"""
    from pathlib import Path
    html_path = Path(__file__).parent.parent / 'esp32-monitor' / 'index.html'
    assert html_path.exists(), f"index.html not found at {html_path}"
    content = html_path.read_text(encoding='utf-8')
    assert '<img id="video-stream"' in content, "Missing video stream img"
    assert 'id="servo-slider"' in content, "Missing servo slider"
    assert 'id="btn-camera-on"' in content, "Missing camera on button"
    assert 'id="btn-camera-off"' in content, "Missing camera off button"


def test_style_css_exists():
    """验证 style.css 存在"""
    from pathlib import Path
    css_path = Path(__file__).parent.parent / 'esp32-monitor' / 'style.css'
    assert css_path.exists(), f"style.css not found at {css_path}"


def test_app_js_exists_and_config():
    """验证 app.js 存在且包含正确的 ESP32 配置"""
    from pathlib import Path
    js_path = Path(__file__).parent.parent / 'esp32-monitor' / 'app.js'
    assert js_path.exists(), f"app.js not found at {js_path}"
    content = js_path.read_text(encoding='utf-8')
    assert 'ESP32_IP' in content, "Missing ESP32_IP config"
    assert 'CONTROL_PORT' in content, "Missing CONTROL_PORT config"
    assert 'STREAM_PORT' in content, "Missing STREAM_PORT config"
    assert '10.0.0.110' in content, "Missing default IP config"


def test_control_get_status(base_url, session):
    """Test GET /control returns current status"""
    resp = session.get(f"{base_url}/control", timeout=10)
    assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"
    data = resp.json()
    assert data.get("status") == "ok", f"Expected status=ok, got {data}"


def test_control_post_camera_on(base_url, session):
    """Test POST /control with action=on"""
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
    """Test POST /control with action=off"""
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
    """Test POST /control with servo angle"""
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
    """Test GET /stream returns MJPEG stream"""
    import requests
    resp = requests.get(f"{stream_url}/stream", timeout=10, stream=True)
    assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"
    assert "multipart" in resp.headers.get("Content-Type", ""), \
        f"Expected multipart content-type"
```

- [ ] **Step 2: 创建 conftest.py**

```python
"""
pytest 配置 for ESP32 监控前端测试
"""

import pytest


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

---

## Task 5: Docker 部署配置

**Files:**
- Create: `esp32-monitor/Dockerfile`
- Create: `esp32-monitor/nginx.conf`
- Create: `esp32-monitor/.dockerignore`

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

        # 禁止访问隐藏文件
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
docker-compose.yml
```

- [ ] **Step 4: 创建 docker-compose.yml（用于二阶段测试）**

```yaml
version: '3.8'
services:
  esp32-monitor:
    build: .
    ports:
      - "8080:80"
    # 环境变量用于覆盖 ESP32 IP（可选）
    environment:
      - ESP32_IP=10.0.0.110
```

---

## Task 6: 提交所有文件

- [ ] **Step 1: 提交 esp32-monitor 目录**

```bash
git add esp32-monitor/
git commit -m "feat: add ESP32 monitor frontend (HTML/CSS/JS)

- index.html: 主页面，MJPEG 视频流 + 摄像头开关 + 舵机滑动条
- style.css: 深色主题样式
- app.js: ESP32 API 调用逻辑，状态轮询
- Dockerfile + nginx.conf: Docker 部署配置

Three phases: local browser → Docker → remote Nginx.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

- [ ] **Step 2: 提交测试文件**

```bash
git add tests/
git commit -m "test: add ESP32 monitor frontend integration tests

pytest tests/test_esp32_monitor_frontend.py -v
Requires ESP32 firmware running at 10.0.0.110:8080.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

## 验证步骤

1. **本地测试（无需 ESP32）**
   ```bash
   # 打开 esp32-monitor/index.html 在浏览器中
   # 验证页面结构和样式加载正常
   ```

2. **连接 ESP32 测试**
   ```bash
   # 确保 ESP32 固件运行在 10.0.0.110:8080
   pytest tests/test_esp32_monitor_frontend.py -v --esp32-ip 10.0.0.110
   ```

3. **Docker 测试（二阶段）**
   ```bash
   docker build -t esp32-monitor ./esp32-monitor
   docker run -d -p 8080:80 esp32-monitor
   # 浏览器打开 http://localhost:8080
   ```

---

## Spec 覆盖检查

- [x] 摄像头开启/关闭功能 → Task 1, 3
- [x] MJPEG 视频流显示 → Task 1, 3
- [x] 舵机角度滑动控制 → Task 1, 3
- [x] 状态轮询更新 → Task 3
- [x] 三阶段部署配置 → Task 5
- [x] pytest 集成测试 → Task 4