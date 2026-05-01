#!/usr/bin/env python3
"""Simple direct ESP32 test server"""
import asyncio
import json
from aiohttp import web

PORT = 11080

esp32_ws = None

async def ws_handler(request):
    global esp32_ws
    ws = web.WebSocketResponse()
    await ws.prepare(request)
    role = request.query.get('role')

    print(f"Client connected role={role}")

    if role == 'esp32':
        esp32_ws = ws

    async for msg in ws:
        if msg.type == web.WSMsgType.TEXT:
            print(f"Received from {role}: {msg.data}")
            try:
                data = json.loads(msg.data)
                if 'status' in data:
                    print(f"  ESP32 status: camera={data.get('camera')} servo={data.get('servo')}")
            except:
                pass

    if ws is esp32_ws:
        esp32_ws = None
        print("ESP32 disconnected")
    return ws

async def send_to_esp32():
    global esp32_ws
    if not esp32_ws:
        print("ESP32 not connected!")
        return
    await esp32_ws.send_str(json.dumps({'cmd': 'camera_on'}))
    print("Sent camera_on")

async def main():
    app = web.Application()
    app.router.add_get('/ws', ws_handler)
    runner = web.AppRunner(app)
    await runner.setup()
    await web.TCPSite(runner, '0.0.0.0', PORT).start()
    print(f"Server running on port {PORT}")
    print("Waiting for ESP32 to connect...")

    while esp32_ws is None:
        await asyncio.sleep(0.5)

    print("ESP32 connected! Sending camera_on...")
    await esp32_ws.send_str(json.dumps({'cmd': 'camera_on'}))
    await asyncio.sleep(2)
    await esp32_ws.send_str(json.dumps({'cmd': 'servo', 'angle': 135}))
    await asyncio.sleep(2)
    await esp32_ws.send_str(json.dumps({'cmd': 'camera_off'}))
    await asyncio.sleep(2)
    print("Done sending commands")

asyncio.run(main())