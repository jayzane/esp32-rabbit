#!/usr/bin/env python3
"""
ESP32 Monitor Server
- HTTP + WebSocket: 11080
"""

import asyncio
import json
import logging
import os
from aiohttp import web, WSMsgType

logging.basicConfig(level=logging.INFO, format='%(asctime)s %(message)s')
logger = logging.getLogger(__name__)

PORT = 11080

esp32_ws = None
frontend_ws = None
BASE_DIR = os.path.dirname(os.path.abspath(__file__))


async def websocket_handler(request):
    global esp32_ws, frontend_ws

    ws = web.WebSocketResponse()
    await ws.prepare(request)

    addr = request.transport.get_extra_info('peername')
    logger.info(f'Client connected from {addr}')

    # Identify by URL query parameter: ?role=esp32 or ?role=frontend
    query = request.query
    my_role = query.get('role', None)  # 'esp32' or 'frontend'

    if my_role == 'esp32':
        esp32_ws = ws
        logger.info(f'Role identified: ESP32 (by URL param)')
    elif my_role == 'frontend':
        frontend_ws = ws
        logger.info(f'Role identified: Frontend (by URL param)')
    else:
        logger.warning(f'Unknown role from {addr}, closing')
        await ws.close()
        return ws

    try:
        async for msg in ws:
            if msg.type == WSMsgType.TEXT:
                data = msg.data
                logger.info(f'Received: {data}')

                try:
                    parsed = json.loads(data)

                    # ESP32 → Frontend: forward status updates
                    if my_role == 'esp32' and 'status' in parsed:
                        if frontend_ws and not frontend_ws.closed:
                            await frontend_ws.send_json({
                                'camera': parsed.get('camera'),
                                'angle': parsed.get('angle') or parsed.get('servo')
                            })
                            logger.info(f'ESP32 → Frontend: {parsed}')

                    # Frontend → ESP32: translate "action" to "cmd" and forward
                    elif my_role == 'frontend' and 'action' in parsed:
                        if esp32_ws and not esp32_ws.closed:
                            action = parsed['action']
                            if action == 'on':
                                forward = json.dumps({'cmd': 'camera_on'})
                            elif action == 'off':
                                forward = json.dumps({'cmd': 'camera_off'})
                            elif action == 'servo':
                                forward = json.dumps({'cmd': 'servo', 'angle': parsed.get('angle', 90)})
                            elif action == 'status':
                                forward = json.dumps({'cmd': 'status'})
                            else:
                                forward = data
                            await esp32_ws.send_str(forward)
                            logger.info(f'Frontend → ESP32: {forward}')
                        else:
                            logger.warning(f'Frontend → ESP32: ESP32 not connected, dropping')

                except json.JSONDecodeError:
                    logger.info(f'Non-JSON message: {data}')

            elif msg.type == WSMsgType.ERROR:
                logger.error(f'WebSocket error: {ws.exception()}')

    finally:
        if ws is esp32_ws:
            esp32_ws = None
            logger.info('ESP32 disconnected')
        if ws is frontend_ws:
            frontend_ws = None
            logger.info('Frontend disconnected')

    return ws


async def handle_index(request):
    path = os.path.join(BASE_DIR, 'index.html')
    with open(path, 'r', encoding='utf-8') as f:
        return web.Response(text=f.read(), content_type='text/html')


async def handle_app(request):
    path = os.path.join(BASE_DIR, 'app.js')
    with open(path, 'r', encoding='utf-8') as f:
        return web.Response(text=f.read(), content_type='application/javascript')


def main():
    app = web.Application()
    app.router.add_get('/', handle_index)
    app.router.add_get('/app.js', handle_app)
    app.router.add_get('/ws', websocket_handler)

    logger.info(f'Server running: http://0.0.0.0:{PORT} + ws://0.0.0.0:{PORT}/ws')

    web.run_app(app, host='0.0.0.0', port=PORT,
                print=None, access_log=None)


if __name__ == '__main__':
    main()
