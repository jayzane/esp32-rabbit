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
    let toastTimer = null;

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
        startPolling();
        updateVideoDisplay();
        updateCameraButtons();
        updateConnectionUI('pending', 'Connecting...');
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

    // ========== API CALLS ==========
    async function setCamera(on) {
        const prevCameraOn = cameraOn;
        cameraOn = on; // Optimistic update
        updateCameraButtons();

        try {
            const resp = await fetch(CONTROL_URL, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ action: on ? 'on' : 'off' }),
                signal: AbortSignal.timeout(REQUEST_TIMEOUT),
            });
            const data = await resp.json();

            if (data.status === 'ok') {
                cameraOn = data.camera === 'on';
                updateVideoDisplay();
                updateCameraButtons();
                updateConnectionUI('ok', 'Connected');
                showToast(`${on ? 'Camera ON' : 'Camera OFF'}`, 'success');
            } else {
                cameraOn = prevCameraOn;
                updateCameraButtons();
                updateVideoDisplay();
                showToast(data.reason || 'Failed', 'error');
            }
        } catch (err) {
            cameraOn = prevCameraOn;
            updateCameraButtons();
            updateVideoDisplay();
            updateConnectionUI('error', 'Connection failed');
            showToast('Connection failed', 'error');
        }
    }

    async function sendServoAngle(angle) {
        const prevAngle = servoAngle;
        servoAngle = angle;

        try {
            const resp = await fetch(CONTROL_URL, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ action: 'servo', angle: angle }),
                signal: AbortSignal.timeout(REQUEST_TIMEOUT),
            });
            const data = await resp.json();

            if (data.status === 'ok') {
                servoAngle = data.angle !== undefined ? data.angle : angle;
                dom.servoSlider.value = servoAngle;
                dom.servoValue.textContent = servoAngle;
            } else {
                servoAngle = prevAngle;
                dom.servoSlider.value = servoAngle;
                dom.servoValue.textContent = servoAngle;
                showToast(data.reason || 'Servo control failed', 'error');
            }
        } catch (err) {
            servoAngle = prevAngle;
            dom.servoSlider.value = servoAngle;
            dom.servoValue.textContent = servoAngle;
            updateConnectionUI('error', 'Connection failed');
            showToast('Connection failed', 'error');
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
                if (data.angle !== undefined) {
                    servoAngle = data.angle;
                }
                if (data.servo !== undefined) {
                    servoAngle = data.servo;
                }
                dom.servoSlider.value = servoAngle;
                dom.servoValue.textContent = servoAngle;
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
            dom.videoStream.src = STREAM_URL;
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
