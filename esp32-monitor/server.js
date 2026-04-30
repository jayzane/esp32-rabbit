const http = require('http');
const fs = require('fs');
const path = require('path');
const WebSocket = require('ws');

const HTTP_PORT = 8080;
const WS_PORT = 11080;

// HTTP server for static files
const httpServer = http.createServer((req, res) => {
    if (req.url === '/' || req.url === '/index.html') {
        const filePath = path.join(__dirname, 'index.html');
        fs.readFile(filePath, (err, data) => {
            if (err) {
                res.writeHead(500);
                res.end('Server error');
                return;
            }
            res.writeHead(200, { 'Content-Type': 'text/html' });
            res.end(data);
        });
    } else if (req.url === '/app.js') {
        const filePath = path.join(__dirname, 'app.js');
        fs.readFile(filePath, (err, data) => {
            if (err) {
                res.writeHead(404);
                res.end('Not found');
                return;
            }
            res.writeHead(200, { 'Content-Type': 'application/javascript' });
            res.end(data);
        });
    } else {
        res.writeHead(404);
        res.end('Not found');
    }
});

// WebSocket server for ESP32
const wss = new WebSocket.Server({ port: WS_PORT });

let esp32Conn = null;
let frontendConn = null;

wss.on('connection', (ws, req) => {
    console.log('Client connected from', req.socket.remoteAddress);

    ws.on('message', (data) => {
        const msg = data.toString();
        console.log('Received:', msg);

        try {
            const parsed = JSON.parse(msg);

            // ESP32 → frontend: forward status updates
            if (parsed.status === 'ok' && parsed.camera !== undefined) {
                if (frontendConn && frontendConn.readyState === WebSocket.OPEN) {
                    frontendConn.send(JSON.stringify({
                        camera: parsed.camera,
                        angle: parsed.angle || parsed.servo
                    }));
                }
            }
            // frontend → ESP32: forward commands
            else if (parsed.action !== undefined && ws === frontendConn) {
                if (esp32Conn && esp32Conn.readyState === WebSocket.OPEN) {
                    esp32Conn.send(JSON.stringify(parsed));
                    console.log('Forwarded to ESP32:', msg);
                }
            }
        } catch (e) {
            console.log('Non-JSON or unknown message:', msg);
        }
    });

    ws.on('close', () => {
        console.log('Client disconnected');
        if (ws === esp32Conn) {
            esp32Conn = null;
        }
        if (ws === frontendConn) {
            frontendConn = null;
        }
    });

    // First connection = ESP32 (sends status on connect)
    // Could be smarter about detection but for now assume first is ESP32
    if (!esp32Conn) {
        esp32Conn = ws;
        console.log('ESP32 connected');
    } else {
        frontendConn = ws;
        console.log('Frontend connected');
    }
});

httpServer.listen(HTTP_PORT, '0.0.0.0', () => {
    console.log(`HTTP server: http://0.0.0.0:${HTTP_PORT}`);
    console.log(`WebSocket server: ws://0.0.0.0:${WS_PORT}`);
});
