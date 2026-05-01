#!/usr/bin/env python3
"""Simple WebSocket echo server for ESP32 debug testing."""
import asyncio
import json
from aiohttp import web

PORT = 11081

async def ws_handler(request):
    ws = web.WebSocketResponse()
    await ws.prepare(request)
    print(f"[SERVER] ESP32 connected from {request.transport.get_extra_info('peername')}")

    async for msg in ws:
        if msg.type == web.WSMsgType.TEXT:
            print(f"[SERVER] Received: {msg.data}")
            # Echo back with wrapper
            try:
                data = json.loads(msg.data)
                echo = {"echo": data}
                await ws.send_str(json.dumps(echo))
                print(f"[SERVER] Sent echo: {echo}")
            except json.JSONDecodeError:
                await ws.send_str(f"{{\"echo\": \"{msg.data}\"}}")
                print(f"[SERVER] Sent echo (raw): {msg.data}")
        elif msg.type == web.WSMsgType.BINARY:
            print(f"[SERVER] Received binary: {msg.data.hex()}")
            await ws.send_bytes(msg.data)

    print("[SERVER] ESP32 disconnected")
    return ws

async def main():
    app = web.Application()
    app.router.add_get('/debug', ws_handler)
    runner = web.AppRunner(app)
    await runner.setup()
    await web.TCPSite(runner, '0.0.0.0', PORT).start()
    print(f"[SERVER] Echo server running on ws://0.0.0.0:{PORT}/debug")
    print("Waiting for ESP32 connection...")

    # Keep alive
    while True:
        await asyncio.sleep(3600)

asyncio.run(main())