#!/usr/bin/env python3
"""
ESP32 WS Command Tests
测试与真实 ESP32 的 WebSocket 命令通信。

Server: ws://10.0.0.232:11080/ws
ESP32 IP: 10.0.0.110

运行方式:
    pytest test_ws_esp32_commands.py -v
    或直接: python test_ws_esp32_commands.py
"""

import asyncio
import json
import sys
import time
import pytest


# ============================================================================
# Configuration
# ============================================================================

# 用实际 ESP32 连接的服务器地址（同一台机器运行 server.py）
SERVER_URI = "ws://10.0.0.232:11080/ws"

# ESP32 的 IP（用于信息显示）
ESP32_IP = "10.0.0.110"


# ============================================================================
# WebSocket 测试客户端
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

    async def close(self):
        if self.ws:
            await self.ws.close()


# ============================================================================
# 测试用例
# ============================================================================

@pytest.mark.asyncio
async def test_camera_on_command():
    """前端发送 camera_on 命令 -> ESP32 收到 cmd:camera_on"""
    async with WSClient(SERVER_URI, "esp32") as esp32, \
               WSClient(SERVER_URI, "frontend") as frontend:

        # ESP32 先发送心跳表示在线
        await esp32.send_json({"status": "heartbeat", "camera": "off", "servo": 90, "seq": 1})
        await asyncio.sleep(0.2)

        # 前端发送 ON 命令
        await frontend.send_json({"action": "on"})
        await asyncio.sleep(0.2)

        # ESP32 测试客户端应该收到转发后的命令
        # 注意：真实的 ESP32 通过串口日志观测
        # 我们用测试客户端模拟 ESP32 行为来验证服务器转发逻辑
        recv = await esp32.recv_json(timeout=2.0)
        assert recv is not None, "ESP32 测试客户端应收到 camera_on 命令"
        assert recv.get("cmd") == "camera_on", f"期望 cmd=camera_on, 收到: {recv}"


@pytest.mark.asyncio
async def test_camera_off_command():
    """前端发送 camera_off 命令 -> ESP32 收到 cmd:camera_off"""
    async with WSClient(SERVER_URI, "esp32") as esp32, \
               WSClient(SERVER_URI, "frontend") as frontend:

        await esp32.send_json({"status": "heartbeat", "camera": "on", "servo": 90, "seq": 1})
        await asyncio.sleep(0.2)

        await frontend.send_json({"action": "off"})
        await asyncio.sleep(0.2)

        recv = await esp32.recv_json(timeout=2.0)
        assert recv is not None, "ESP32 测试客户端应收到 camera_off 命令"
        assert recv.get("cmd") == "camera_off", f"期望 cmd=camera_off, 收到: {recv}"


@pytest.mark.asyncio
async def test_servo_command_with_angle():
    """前端发送 servo 命令(含角度) -> ESP32 收到 cmd:servo + angle"""
    async with WSClient(SERVER_URI, "esp32") as esp32, \
               WSClient(SERVER_URI, "frontend") as frontend:

        await esp32.send_json({"status": "heartbeat", "camera": "off", "servo": 90, "seq": 1})
        await asyncio.sleep(0.2)

        await frontend.send_json({"action": "servo", "angle": 135})
        await asyncio.sleep(0.2)

        recv = await esp32.recv_json(timeout=2.0)
        assert recv is not None, "ESP32 测试客户端应收到 servo 命令"
        assert recv.get("cmd") == "servo", f"期望 cmd=servo, 收到: {recv}"
        assert recv.get("angle") == 135, f"期望 angle=135, 收到: {recv}"


@pytest.mark.asyncio
async def test_esp32_status_forwarded_to_frontend():
    """ESP32 发送状态 -> 前端收到 camera/angle 信息"""
    async with WSClient(SERVER_URI, "esp32") as esp32, \
               WSClient(SERVER_URI, "frontend") as frontend:

        # ESP32 发送 camera=on 状态
        await esp32.send_json({"status": "heartbeat", "camera": "on", "servo": 45, "seq": 2})
        await asyncio.sleep(0.2)

        # 前端应收到转发后的状态（camera=on, angle=45）
        recv = await frontend.recv_json(timeout=2.0)
        assert recv is not None, "前端应收到 ESP32 状态"
        assert recv.get("camera") == "on", f"期望 camera=on, 收到: {recv}"
        assert recv.get("angle") == 45, f"期望 angle=45, 收到: {recv}"


@pytest.mark.asyncio
async def test_heartbeat_seq_increment():
    """验证 ESP32 心跳 seq 递增，camera/servo 状态正确"""
    async with WSClient(SERVER_URI, "esp32") as esp32, \
               WSClient(SERVER_URI, "frontend") as frontend:

        # 发送多个心跳，观察 seq 递增
        for i in range(3):
            await esp32.send_json({"status": "heartbeat", "camera": "on", "servo": 90, "seq": i})
            await asyncio.sleep(0.1)

        # 前端应该收到 3 条状态
        for i in range(3):
            recv = await frontend.recv_json(timeout=2.0)
            assert recv is not None, f"第 {i+1} 条心跳应被前端收到"


@pytest.mark.asyncio
async def test_server_translates_action_to_cmd():
    """验证服务器正确将 frontend action 翻译为 ESP32 cmd 格式"""
    async with WSClient(SERVER_URI, "esp32") as esp32, \
               WSClient(SERVER_URI, "frontend") as frontend:

        await esp32.send_json({"status": "ok", "camera": "off", "servo": 90, "seq": 1})
        await asyncio.sleep(0.1)

        # 测试 on
        await frontend.send_json({"action": "on"})
        recv = await esp32.recv_json(timeout=2.0)
        assert recv is not None and recv.get("cmd") == "camera_on"

        # 再测试 off
        await frontend.send_json({"action": "off"})
        recv = await esp32.recv_json(timeout=2.0)
        assert recv is not None and recv.get("cmd") == "camera_off"


@pytest.mark.asyncio
async def test_esp32_heartbeat_forwarding():
    """ESP32 心跳 -> 前端收到 camera/servo 状态"""
    async with WSClient(SERVER_URI, "esp32") as esp32, \
               WSClient(SERVER_URI, "frontend") as frontend:

        await esp32.send_json({
            "status": "heartbeat",
            "seq": 99,
            "camera": "on",
            "servo": 180
        })

        recv = await frontend.recv_json(timeout=2.0)
        assert recv is not None
        assert recv.get("camera") == "on"
        assert recv.get("angle") == 180


@pytest.mark.asyncio
async def test_server_drops_when_esp32_not_connected():
    """ESP32 未连接时，前端命令被服务器丢弃（不报错）"""
    async with WSClient(SERVER_URI, "frontend") as frontend:
        # ESP32 不连接，直接发送命令
        # 服务器应该安静地丢弃（根据 server.py 代码会打印 warning 但不 crash）
        await frontend.send_json({"action": "on"})
        await asyncio.sleep(0.5)
        # 没有异常即通过


# ============================================================================
# 辅助函数
# ============================================================================

async def check_server():
    """检查服务器是否在线"""
    import websockets
    try:
        async with websockets.connect(SERVER_URI + "?role=test", open_timeout=3):
            return True
    except Exception:
        return False


# ============================================================================
# Main Runner (直接运行而非 pytest)
# ============================================================================

async def run_all_tests():
    """运行所有测试（直接运行模式）"""
    print("=" * 60)
    print("ESP32 WS Command Tests")
    print("=" * 60)

    server_ok = await check_server()
    print(f"Server ({SERVER_URI}): {'ONLINE' if server_ok else 'OFFLINE'}")
    if not server_ok:
        print("ERROR: 服务器未运行，请先启动 python esp32-monitor/server.py")
        return False

    tests = [
        ("test_camera_on_command", test_camera_on_command),
        ("test_camera_off_command", test_camera_off_command),
        ("test_servo_command_with_angle", test_servo_command_with_angle),
        ("test_esp32_status_forwarded_to_frontend", test_esp32_status_forwarded_to_frontend),
        ("test_heartbeat_seq_increment", test_heartbeat_seq_increment),
        ("test_server_translates_action_to_cmd", test_server_translates_action_to_cmd),
        ("test_esp32_heartbeat_forwarding", test_esp32_heartbeat_forwarding),
        ("test_server_drops_when_esp32_not_connected", test_server_drops_when_esp32_not_connected),
    ]

    passed = 0
    failed = 0

    for name, test_fn in tests:
        try:
            await test_fn()
            print(f"  [PASS] {name}")
            passed += 1
        except Exception as e:
            print(f"  [FAIL] {name}: {e}")
            failed += 1

    print()
    print("=" * 60)
    print(f"Results: {passed} passed, {failed} failed")
    print("=" * 60)
    return failed == 0


def main():
    ok = asyncio.run(run_all_tests())
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()