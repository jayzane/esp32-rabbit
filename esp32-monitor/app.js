/**
 * ESP32 Monitor - Frontend Logic
 * WebSocket client connected to ESP32 via ws://10.0.0.232:11080
 */

(function() {
    'use strict';

    // ========== CONFIG ==========
    const WS_HOST = '10.0.0.232';
    const WS_PORT = 11080;
    const WS_URL = `ws://${WS_HOST}:${WS_PORT}/ws?role=frontend`;
    const POLL_INTERVAL = 5000;
    const RECONNECT_DELAY = 3000;

    // ========== STATE ==========
    let cameraOn = false;
    let servoAngle = 90;
    let ws = null;
    let pollTimer = null;
    let toastTimer = null;
    let connected = false;

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
        toast: document.getElementById('toast'),
    };

    // ========== INIT ==========
    function init() {
        setupEventListeners();
        connectWS();
        updateVideoDisplay();
        updateCameraButtons();
        updateConnectionUI('pending', 'Connecting...');
    }

    // ========== WEBSOCKET ==========
    function connectWS() {
        ws = new WebSocket(WS_URL);

        ws.onopen = () => {
            console.log('WebSocket connected');
            connected = true;
            updateConnectionUI('ok', 'Connected');
            // Request status on connect
            send({ action: 'status' });
        };

        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                if (data.camera !== undefined) {
                    cameraOn = data.camera === 'on';
                    updateVideoDisplay();
                    updateCameraButtons();
                }
                if (data.angle !== undefined) {
                    servoAngle = data.angle;
                    dom.servoSlider.value = servoAngle;
                    dom.servoValue.textContent = servoAngle;
                }
                if (data.servo !== undefined) {
                    servoAngle = data.servo;
                    dom.servoSlider.value = servoAngle;
                    dom.servoValue.textContent = servoAngle;
                }
            } catch (e) {
                console.log('WS message:', event.data);
            }
        };

        ws.onclose = () => {
            console.log('WebSocket disconnected');
            connected = false;
            updateConnectionUI('error', 'Disconnected');
            // Reconnect after delay
            setTimeout(connectWS, RECONNECT_DELAY);
        };

        ws.onerror = (err) => {
            console.log('WebSocket error', err);
            updateConnectionUI('error', 'Connection failed');
        };
    }

    function send(data) {
        console.log('send called with:', data);
        if (ws && ws.readyState === WebSocket.OPEN) {
            console.log('WebSocket sending:', JSON.stringify(data));
            ws.send(JSON.stringify(data));
        } else {
            console.log('WebSocket not ready, state:', ws ? ws.readyState : 'ws is null');
        }
    }

    // ========== EVENT LISTENERS ==========
    function setupEventListeners() {
        dom.btnCameraOn.addEventListener('click', () => setCamera(true));
        dom.btnCameraOff.addEventListener('click', () => setCamera(false));

        dom.servoSlider.addEventListener('input', (e) => {
            const val = parseInt(e.target.value, 10);
            dom.servoValue.textContent = val;
        });

        dom.servoSlider.addEventListener('change', (e) => {
            const val = parseInt(e.target.value, 10);
            sendServoAngle(val);
        });
    }

    // ========== ACTIONS ==========
    function setCamera(on) {
        const msg = { action: on ? 'on' : 'off' };
        console.log('setCamera called:', msg);
        send(msg);
        showToast(`${on ? 'Camera ON' : 'Camera OFF'}`, 'info');
    }

    function sendServoAngle(angle) {
        send({ action: 'servo', angle: angle });
    }

    // ========== UI UPDATES ==========
    function updateVideoDisplay() {
        if (cameraOn) {
            dom.videoStream.src = `http://${WS_HOST}:11081/stream`;
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

    // ========== TOAST ==========
    function showToast(message, type = 'info') {
        if (toastTimer) clearTimeout(toastTimer);

        dom.toast.textContent = message;
        dom.toast.className = `toast ${type} show`;

        toastTimer = setTimeout(() => {
            dom.toast.classList.remove('show');
        }, 3000);
    }

    // ========== START ==========
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }
})();
