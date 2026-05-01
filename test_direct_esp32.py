#!/usr/bin/env python3
"""Direct ESP32 test server - acts as WS server, ESP32 connects as client"""
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
        print("ESP32 connected, will send commands in 2s...")

    try:
        async for msg in ws:
            if msg.type == web.WSMsgType.TEXT:
                print(f"Received from {role}: {msg.data}")
                try:
                    data = json.loads(msg.data)
                    if 'status' in data:
                        print(f"  -> camera={data.get('camera')} servo={data.get('servo')}")
                except:
                    pass
    except Exception as e:
        print(f"WS error: {e}")
    finally:
        if ws is esp32_ws:
            esp32_ws = None
            print("ESP32 disconnected")
    return ws

async def main():
    global esp32_ws
    app = web.Application()
    app.router.add_get('/ws', ws_handler)
    runner = web.AppRunner(app)
    await runner.setup()
    await web.TCPSite(runner, '0.0.0.0', PORT).start()
    print(f"Server running on port {PORT}")

    # Wait for ESP32
    while esp32_ws is None:
        print("Waiting for ESP32...")
        await asyncio.sleep(0.5)

    print("ESP32 connected! Waiting 2s to stabilize...")
    await asyncio.sleep(2)

    # Send commands
    commands = [
        {'cmd': 'camera_on'},
        {'cmd': 'camera_off'},
        {'cmd': 'servo', 'angle': 90},
        {'cmd': 'servo', 'angle': 135},
        {'cmd': 'camera_on'},
    ]

    for i, cmd in enumerate(commands):
        if esp32_ws is None or esp32_ws.closed:
            print(f"ESP32 disconnected at command {i}")
            break
        print(f"[{i+1}] Sending: {cmd}")
        try:
            await esp32_ws.send_str(json.dumps(cmd))
        except Exception as e:
            print(f"Send failed: {e}")
            break
        await asyncio.sleep(3)

    print("Done")
    await asyncio.sleep(1)
    await runner.cleanup()

asyncio.run(main())