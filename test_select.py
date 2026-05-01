#!/usr/bin/env python3
"""
Simulate ESP32 recv task behavior - tests if select() sees incoming data.
"""
import socket
import time
import threading
import json

def test_recv_behavior():
    # Create server socket
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('127.0.0.1', 19999))
    server.listen(1)
    server.settimeout(0.5)

    def client_sender():
        time.sleep(0.3)
        print("[test] Client connecting...")
        c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        c.connect(('127.0.0.1', 19999))
        print("[test] Client connected, waiting before send...")
        time.sleep(0.5)
        print("[test] Client sending data...")
        c.sendall(b'hello')
        time.sleep(0.1)
        c.close()

    t = threading.Thread(target=client_sender)
    t.start()

    print("[test] Server accepting...")
    try:
        conn, addr = server.accept()
        print(f"[test] Accepted from {addr}")
    except socket.timeout:
        print("[test] Accept timeout!")
        server.close()
        t.join()
        return

    # Simulate select() behavior
    print("[test] Calling select() with 2s timeout...")
    import select
    readable, _, _ = select.select([conn], [], [], 2.0)
    if readable:
        data = conn.recv(1024)
        print(f"[test] Received: {data}")
    else:
        print("[test] select timeout - no data available!")

    conn.close()
    server.close()
    t.join()

if __name__ == "__main__":
    test_recv_behavior()