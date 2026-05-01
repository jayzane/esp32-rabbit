#!/usr/bin/env python3
"""
Direct ESP32 WS Server Test
扮演 WebSocket 服务器，直接给 ESP32 发命令，验证 camera 和 servo。

ESP32 作为客户端连接到此服务器。
运行此脚本后，ESP32 重启或重连时会连接到此服务器。

Usage:
    python test_direct_esp32_server.py
"""

import asyncio
import json
import sys
import time
from aiohttp import web, WSMsgType


# ============================================================================
# Configuration
# ============================================================================

# 此脚本作为服务器，ESP32 连接到此
SERVER_HOST = "0.0.0.0"
SERVER_PORT = 11080
WS_PATH = "/ws"


# ============================================================================
# WebSocket Server
# ============================================================================

class ESP32WSServer:
    def __init__(self):
        self.esp32_ws = None
        self.frontend_ws = None

    async def handle_websocket(self, request):
        """处理 WebSocket 连接，识别 role 参数"""
        ws = web.WebSocketResponse()
        await ws.prepare(request)

        addr = request.transport.get_extra_info('peername')
        print(f"[SERVER] Client connected from {addr}")

        query = request.query
        role = query.get('role', None)

        if role == 'esp32':
            self.esp32_ws = ws
            print(f"[SERVER] Role: ESP32")
        elif role == 'frontend':
            self.frontend_ws = ws
            print(f"[SERVER] Role: Frontend")
        else:
            print(f"[SERVER] Unknown role, closing")
            await ws.close()
            return ws

        # 处理消息
        async for msg in ws:
            if msg.type == WSMsgType.TEXT:
                data = msg.data
                print(f"[SERVER] Received from {role}: {data}")

                # ESP32 发送的状态 -> 转发给 frontend
                if role == 'esp32' and self.frontend_ws:
                    try:
                        parsed = json.loads(data)
                        await self.frontend_ws.send_json({
                            'camera': parsed.get('camera'),
                            'angle': parsed.get('angle') or parsed.get('servo')
                        })
                    except:
                        pass

                # Frontend 发来的命令 -> 转发给 ESP32
                elif role == 'frontend' and self.esp32_ws:
                    try:
                        parsed = json.loads(data)
                        if 'action' in parsed:
                            action = parsed['action']
                            if action == 'on':
                                forward = json.dumps({'cmd': 'camera_on'})
                            elif action == 'off':
                                forward = json.dumps({'cmd': 'camera_off'})
                            elif action == 'servo':
                                forward = json.dumps({'cmd': 'servo', 'angle': parsed.get('angle', 90)})
                            else:
                                forward = data

                            await self.esp32_ws.send_str(forward)
                            print(f"[SERVER] Forwarded to ESP32: {forward}")
                    except:
                        pass

        # 连接关闭
        if ws is self.esp32_ws:
            self.esp32_ws = None
            print(f"[SERVER] ESP32 disconnected")
        if ws is self.frontend_ws:
            self.frontend_ws = None
            print(f"[SERVER] Frontend disconnected")

        return ws


# ============================================================================
# 测试用例：直接给 ESP32 发命令
# ============================================================================

async def test_camera_on(server, timeout=5.0):
    """发送 camera_on 命令"""
    if not server.esp32_ws:
        print("[TEST] ESP32 not connected!")
        return False

    print("[TEST] Sending camera_on command...")
    await server.esp32_ws.send_str(json.dumps({'cmd': 'camera_on'}))
    print("[TEST] camera_on sent")
    return True


async def test_camera_off(server, timeout=5.0):
    """发送 camera_off 命令"""
    if not server.esp32_ws:
        print("[TEST] ESP32 not connected!")
        return False

    print("[TEST] Sending camera_off command...")
    await server.esp32_ws.send_str(json.dumps({'cmd': 'camera_off'}))
    print("[TEST] camera_off sent")
    return True


async def test_servo(server, angle=135, timeout=5.0):
    """发送 servo 命令"""
    if not server.esp32_ws:
        print("[TEST] ESP32 not connected!")
        return False

    print(f"[TEST] Sending servo command with angle={angle}...")
    await server.esp32_ws.send_str(json.dumps({'cmd': 'servo', 'angle': angle}))
    print(f"[TEST] servo angle={angle} sent")
    return True


# ============================================================================
# Main: 交互式测试
# ============================================================================

async def interactive_test():
    """交互式发送命令到 ESP32"""
    server = ESP32WSServer()

    async def ws_handler(request):
        return await server.handle_websocket(request)

    app = web.Application()
    app.router.add_get(WS_PATH, ws_handler)

    print("=" * 60)
    print("ESP32 Direct WS Server")
    print("=" * 60)
    print(f"Server: ws://0.0.0.0:{SERVER_PORT}{WS_PATH}")
    print(f"ESP32 连接时需使用: ws://<本机IP>:{SERVER_PORT}{WS_PATH}?role=esp32")
    print("=" * 60)
    print("Commands:")
    print("  1 - camera_on")
    print("  2 - camera_off")
    print("  3 - servo 90")
    print("  4 - servo 135")
    print("  5 - servo 45")
    print("  q - quit")
    print("=" * 60)

    # 启动服务器
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, SERVER_HOST, SERVER_PORT)
    await site.start()
    print(f"[SERVER] Started on {SERVER_HOST}:{SERVER_PORT}")

    # 等待 ESP32 连接
    print("[SERVER] Waiting for ESP32 to connect...")
    while server.esp32_ws is None:
        await asyncio.sleep(0.5)

    print("[SERVER] ESP32 connected!")
    print("[SERVER] Ready to send commands")
    print()

    # 交互循环
    while True:
        print("> ", end="", flush=True)
        try:
            line = await asyncio.get_event_loop().run_in_executor(None, sys.stdin.readline)
        except:
            break

        line = line.strip()
        if not line:
            continue

        if line == 'q':
            break
        elif line == '1':
            await test_camera_on(server)
        elif line == '2':
            await test_camera_off(server)
        elif line == '3':
            await test_servo(server, 90)
        elif line == '4':
            await test_servo(server, 135)
        elif line == '5':
            await test_servo(server, 45)
        else:
            print("Unknown command")

    await runner.cleanup()


# ============================================================================
# Auto Test: 自动测试所有命令
# ============================================================================

async def auto_test():
    """自动发送所有命令并观察 ESP32 响应"""
    server = ESP32WSServer()

    async def ws_handler(request):
        return await server.handle_websocket(request)

    app = web.Application()
    app.router.add_get(WS_PATH, ws_handler)

    print("=" * 60)
    print("ESP32 Direct WS Server - Auto Test Mode")
    print("=" * 60)

    # 启动服务器
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, SERVER_HOST, SERVER_PORT)
    await site.start()

    print(f"[SERVER] Started on {SERVER_HOST}:{SERVER_PORT}")
    print("[SERVER] Waiting for ESP32...")

    # 等待 ESP32
    start = time.time()
    while server.esp32_ws is None:
        if time.time() - start > 30:
            print("[ERROR] Timeout waiting for ESP32")
            await runner.cleanup()
            return
        await asyncio.sleep(0.5)

    print("[SERVER] ESP32 connected!")

    # 等待一秒让 ESP32 稳定
    await asyncio.sleep(1)

    # 发送测试命令
    tests = [
        ("camera_on", lambda: server.esp32_ws.send_str(json.dumps({'cmd': 'camera_on'}))),
        ("camera_off", lambda: server.esp32_ws.send_str(json.dumps({'cmd': 'camera_off'}))),
        ("servo 90", lambda: server.esp32_ws.send_str(json.dumps({'cmd': 'servo', 'angle': 90}))),
        ("servo 135", lambda: server.esp32_ws.send_str(json.dumps({'cmd': 'servo', 'angle': 135}))),
        ("servo 45", lambda: server.esp32_ws.send_str(json.dumps({'cmd': 'servo', 'angle': 45}))),
        ("camera_on", lambda: server.esp32_ws.send_str(json.dumps({'cmd': 'camera_on'}))),
    ]

    for i, (name, fn) in enumerate(tests):
        print(f"[TEST {i+1}] Sending {name}...")
        await fn()
        await asyncio.sleep(2)

    print("[TEST] All commands sent")
    print("[TEST] Watch ESP32 serial output for responses")

    # 再等一会儿看 ESP32 的响应
    await asyncio.sleep(3)

    await runner.cleanup()


# ============================================================================
# Entry Point
# ============================================================================

def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--auto':
        asyncio.run(auto_test())
    else:
        asyncio.run(interactive_test())


if __name__ == "__main__":
    main()