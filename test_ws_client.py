#!/usr/bin/env python3
"""Test WebSocket client to verify server message forwarding."""
import asyncio
import json
import websockets

async def test():
    uri = "ws://10.0.0.232:11080/ws"

    print(f"Connecting to {uri}...")
    async with websockets.connect(uri) as ws:
        # Receive first message (should be from server greeting or status)
        msg = await ws.recv()
        print(f"Received: {msg}")

        # Send as if from ESP32
        print("Sending as ESP32: {\"status\": \"ok\", \"camera\": \"off\", \"servo\": 90}")
        await ws.send(json.dumps({"status": "ok", "camera": "off", "servo": 90}))

        # Send as if from Frontend
        print("Sending as Frontend: {\"action\": \"on\"}")
        await ws.send(json.dumps({"action": "on"}))

        # Wait for response
        try:
            msg = await asyncio.wait_for(ws.recv(), timeout=3)
            print(f"Received response: {msg}")
        except asyncio.TimeoutError:
            print("No response received within 3s")

if __name__ == "__main__":
    asyncio.run(test())
