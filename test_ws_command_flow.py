#!/usr/bin/env python3
"""
Verify server forwards commands to ESP32 by pretending to be ESP32.
"""
import asyncio
import json
import sys

SERVER_URI = "ws://127.0.0.1:11080/ws"

async def test_esp32_receives():
    import websockets
    import urllib.parse

    # Connect as ESP32 and wait for commands
    parsed = urllib.parse.urlparse(SERVER_URI)
    path = parsed.path or "/"
    uri_esp32 = f"{parsed.scheme}://{parsed.hostname}:{parsed.port}{path}?role=esp32"

    print(f"Connecting as ESP32 to {uri_esp32}...")
    async with websockets.connect(uri_esp32) as ws:
        print("ESP32 connected, waiting for commands...")

        # First send a status so server knows we're connected
        await ws.send(json.dumps({"status": "ok", "camera": "off", "servo": 90}))
        print("Sent initial status")

        # Wait for commands for 10 seconds
        for i in range(10):
            try:
                msg = await asyncio.wait_for(ws.recv(), timeout=1.0)
                print(f"[{i}] Received command: {msg}")
            except asyncio.TimeoutError:
                print(f"[{i}] Still waiting... (no data)")
        print("Done")

async def test_frontend_sends():
    import websockets
    import urllib.parse

    parsed = urllib.parse.urlparse(SERVER_URI)
    path = parsed.path or "/"
    uri_fe = f"{parsed.scheme}://{parsed.hostname}:{parsed.port}{path}?role=frontend"

    await asyncio.sleep(0.5)  # Let ESP32 connect first

    print(f"\nConnecting as frontend to {uri_fe}...")
    async with websockets.connect(uri_fe) as ws:
        print("Frontend connected, sending camera_on...")
        await ws.send(json.dumps({"action": "on"}))
        print("Sent camera_on")
        await asyncio.sleep(1)
        print("Done sending")

async def main():
    await asyncio.gather(
        test_esp32_receives(),
        test_frontend_sends(),
    )

if __name__ == "__main__":
    asyncio.run(main())