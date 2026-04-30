#!/usr/bin/env python3
"""
ESP32 Integration Test Suite
Tests WebSocket communication between ESP32, frontend, and server.

Server implementations tested:
- Python server (server.py): ws://localhost:11080/ws
- Node.js server (server.js): ws://localhost:11080

Test scenarios:
1. ESP32 connects and sends status/heartbeat
2. Frontend connects and receives ESP32 status
3. Frontend sends commands → server translates → ESP32 receives
4. Connection lifecycle

Run:
    python test_esp32_integration.py          # Check servers
    python -m pytest test_esp32_integration.py -v  # Run all tests
"""

import asyncio
import json
import time
import sys
from typing import Optional, Tuple


# ============================================================================
# Test Configuration
# ============================================================================

PYTHON_SERVER_URI = "ws://127.0.0.1:11080/ws"
NODE_SERVER_URI = "ws://127.0.0.1:11080"


# ============================================================================
# WebSocket Client Helpers
# ============================================================================

class WSClient:
    """Async WebSocket client with role support."""

    def __init__(self, uri: str, role: str = "esp32"):
        self.uri = uri
        self.role = role
        self.ws = None
        self.connected = False
        self.messages = []

    async def __aenter__(self):
        import websockets
        import urllib.parse
        parsed = urllib.parse.urlparse(self.uri)
        path = parsed.path or "/"
        uri_with_role = f"{parsed.scheme}://{parsed.hostname}:{parsed.port}{path}?role={self.role}"
        self.ws = await websockets.connect(uri_with_role)
        self.connected = True
        return self

    async def __aexit__(self, *args):
        if self.ws:
            await self.ws.close()
        self.connected = False

    async def send_json(self, data: dict):
        msg = json.dumps(data)
        self.messages.append(("sent", msg))
        await self.ws.send(msg)

    async def recv_json(self, timeout: float = 3.0):
        try:
            msg = await asyncio.wait_for(self.ws.recv(), timeout=timeout)
            self.messages.append(("recv", msg))
            return json.loads(msg)
        except asyncio.TimeoutError:
            return None

    async def recv_raw(self, timeout: float = 0.5):
        try:
            msg = await asyncio.wait_for(self.ws.recv(), timeout=timeout)
            self.messages.append(("recv", msg))
            return msg
        except asyncio.TimeoutError:
            return None


# ============================================================================
# Test Functions (async)
# ============================================================================

async def test_esp32_connect_and_status():
    """ESP32 connects and sends status, server doesn't reply to ESP32."""
    async with WSClient(PYTHON_SERVER_URI, "esp32") as esp32:
        await esp32.send_json({"status": "ok", "camera": "off", "servo": 90, "seq": 1})
        # Server should NOT send anything back to ESP32
        recv = await esp32.recv_raw(timeout=0.5)
        assert recv is None, f"Server should not reply to ESP32, got: {recv}"
    return True, "ESP32 connected and sent status OK"


async def test_frontend_receives_esp32_status():
    """ESP32 sends status → frontend receives forwarded state."""
    async with WSClient(PYTHON_SERVER_URI, "esp32") as esp32, \
               WSClient(PYTHON_SERVER_URI, "frontend") as frontend:

        await esp32.send_json({"status": "ok", "camera": "on", "servo": 45, "seq": 1})

        # Frontend should receive forwarded status
        msg = await frontend.recv_json(timeout=2.0)
        assert msg is not None, "Frontend should receive ESP32 status"
        assert msg["camera"] == "on"
        assert msg["angle"] == 45
    return True, "Frontend received ESP32 status forward OK"


async def test_camera_on_command():
    """Frontend 'action: on' → server translates → ESP32 receives 'cmd: camera_on'."""
    async with WSClient(PYTHON_SERVER_URI, "esp32") as esp32, \
               WSClient(PYTHON_SERVER_URI, "frontend") as frontend:

        await esp32.send_json({"status": "ok", "camera": "off", "seq": 1})
        await frontend.send_json({"action": "on"})

        cmd = await esp32.recv_json(timeout=2.0)
        assert cmd is not None, "ESP32 should receive camera_on command"
        assert cmd["cmd"] == "camera_on"
    return True, "camera_on command forwarded OK"


async def test_camera_off_command():
    """Frontend 'action: off' → server translates → ESP32 receives 'cmd: camera_off'."""
    async with WSClient(PYTHON_SERVER_URI, "esp32") as esp32, \
               WSClient(PYTHON_SERVER_URI, "frontend") as frontend:

        await esp32.send_json({"status": "ok", "camera": "on", "seq": 1})
        await frontend.send_json({"action": "off"})

        cmd = await esp32.recv_json(timeout=2.0)
        assert cmd is not None, "ESP32 should receive camera_off command"
        assert cmd["cmd"] == "camera_off"
    return True, "camera_off command forwarded OK"


async def test_servo_command():
    """Frontend sends servo angle → server forwards to ESP32."""
    async with WSClient(PYTHON_SERVER_URI, "esp32") as esp32, \
               WSClient(PYTHON_SERVER_URI, "frontend") as frontend:

        await esp32.send_json({"status": "ok", "servo": 90, "seq": 1})
        await frontend.send_json({"action": "servo", "angle": 135})

        cmd = await esp32.recv_json(timeout=2.0)
        assert cmd is not None, "ESP32 should receive servo command"
        assert cmd["cmd"] == "servo"
        assert cmd["angle"] == 135
    return True, "servo command with angle forwarded OK"


async def test_heartbeat_forwarded():
    """ESP32 sends heartbeat → frontend receives camera/servo state."""
    async with WSClient(PYTHON_SERVER_URI, "esp32") as esp32, \
               WSClient(PYTHON_SERVER_URI, "frontend") as frontend:

        await esp32.send_json({"status": "heartbeat", "camera": "on", "servo": 90, "seq": 5})

        msg = await frontend.recv_json(timeout=2.0)
        assert msg is not None, "Frontend should receive heartbeat"
        assert msg["camera"] == "on"
        assert "angle" in msg
    return True, "Heartbeat forwarded to frontend OK"


async def test_late_frontend_no_replay():
    """Frontend connecting after status is sent gets no message replay."""
    async with WSClient(PYTHON_SERVER_URI, "esp32") as esp32:
        # ESP32 sends status before frontend connects
        await esp32.send_json({"status": "ok", "camera": "on", "seq": 1})

        # Frontend connects late
        async with WSClient(PYTHON_SERVER_URI, "frontend") as frontend:
            recv = await frontend.recv_json(timeout=0.5)
            assert recv is None, "No message persistence for late frontend"
    return True, "Late frontend doesn't receive old status OK"


async def test_node_server_esp32_connect():
    """Test ESP32 connection to Node.js server."""
    async with WSClient(NODE_SERVER_URI, "esp32") as esp32:
        await esp32.send_json({"status": "ok", "camera": "off", "servo": 90, "seq": 1})
        recv = await esp32.recv_raw(timeout=0.5)
        assert recv is None
    return True, "Node.js server ESP32 connection OK"


async def test_node_server_command_forwarding():
    """Test frontend→ESP32 command forwarding on Node.js server."""
    async with WSClient(NODE_SERVER_URI, "esp32") as esp32, \
               WSClient(NODE_SERVER_URI, "frontend") as frontend:

        await esp32.send_json({"status": "ok", "camera": "off", "seq": 1})
        await frontend.send_json({"action": "on"})

        cmd = await esp32.recv_json(timeout=2.0)
        assert cmd is not None, "ESP32 should receive camera_on command"
        assert cmd["cmd"] == "camera_on"
    return True, "Node.js server command forwarding OK"


# ============================================================================
# Test Registry
# ============================================================================

TESTS = {
    "python": [
        ("test_esp32_connect_and_status", test_esp32_connect_and_status),
        ("test_frontend_receives_esp32_status", test_frontend_receives_esp32_status),
        ("test_camera_on_command", test_camera_on_command),
        ("test_camera_off_command", test_camera_off_command),
        ("test_servo_command", test_servo_command),
        ("test_heartbeat_forwarded", test_heartbeat_forwarded),
        ("test_late_frontend_no_replay", test_late_frontend_no_replay),
    ],
    "node": [
        ("test_node_server_esp32_connect", test_node_server_esp32_connect),
        ("test_node_server_command_forwarding", test_node_server_command_forwarding),
    ],
}


# ============================================================================
# Test Runner
# ============================================================================

async def check_servers():
    """Check which servers are running."""
    import websockets

    py_ok = False
    node_ok = False

    try:
        async with websockets.connect(PYTHON_SERVER_URI + "?role=test", open_timeout=2):
            py_ok = True
    except:
        pass

    try:
        async with websockets.connect(NODE_SERVER_URI + "?role=test", open_timeout=2):
            node_ok = True
    except:
        pass

    return py_ok, node_ok


def run_tests(tests, server_name):
    """Run a list of async test functions."""
    results = []
    for name, fn in tests:
        try:
            ok, msg = asyncio.run(fn())
            results.append((name, "PASS", msg))
        except Exception as e:
            results.append((name, "FAIL", str(e)))
    return results


def main():
    py_ok, node_ok = asyncio.run(check_servers())

    print("=" * 60)
    print("ESP32 Integration Test Suite")
    print("=" * 60)
    print(f"Python server (ws://127.0.0.1:11080/ws): {'RUNNING' if py_ok else 'NOT RUNNING'}")
    print(f"Node.js server (ws://127.0.0.1:11080):      {'RUNNING' if node_ok else 'NOT RUNNING'}")
    print("=" * 60)

    all_passed = True

    if py_ok:
        print("\n--- Python Server Tests ---")
        results = run_tests(TESTS["python"], "Python")
        for name, status, msg in results:
            print(f"  [{status}] {name}: {msg}")
            if status != "PASS":
                all_passed = False
    else:
        print("\n[SKIP] Python server not running")

    if node_ok:
        print("\n--- Node.js Server Tests ---")
        results = run_tests(TESTS["node"], "Node")
        for name, status, msg in results:
            print(f"  [{status}] {name}: {msg}")
            if status != "PASS":
                all_passed = False
    else:
        print("\n[SKIP] Node.js server not running")

    if not py_ok and not node_ok:
        print("\nERROR: No servers running!")
        print("Start at least one server:")
        print("  Python:  python esp32-monitor/server.py")
        print("  Node.js: node esp32-monitor/server.js")
        sys.exit(1)

    print()
    print("=" * 60)
    print(f"Overall: {'ALL PASSED' if all_passed else 'SOME FAILED'}")
    print("=" * 60)

    if not all_passed:
        sys.exit(1)


if __name__ == "__main__":
    main()
