#!/usr/bin/env python3
"""Read serial port and save to file."""
import serial
import sys
import time

port = serial.Serial('COM8', 115200, timeout=3)
port.flushInput()

print("Reading serial for 10 seconds...")
with open('serial_log.txt', 'w') as f:
    start = __import__('time').time()
    while time.time() - start < 10:
        data = port.read(1024)
        if data:
            text = data.decode('utf-8', errors='replace')
            print(text, end='', flush=True)
            f.write(text)
            f.flush()

port.close()
print("\nDone. Saved to serial_log.txt")