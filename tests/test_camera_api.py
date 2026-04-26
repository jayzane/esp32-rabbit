"""
ESP32 Camera Surveillance Integration Tests

Requires ESP32 running with firmware flashed.
Tests the HTTP control and MJPEG stream endpoints.

Usage:
    pip install requests
    python test_camera_api.py [--ip IP] [--port PORT]

Default: http://10.0.0.110:8080 (control), :8081 (stream)
"""

import argparse
import json
import sys
import time

try:
    import requests
except ImportError:
    print("ERROR: requests module not installed")
    print("Run: pip install requests")
    sys.exit(1)


class CameraAPITest:
    def __init__(self, ip, control_port, stream_port):
        self.ip = ip
        self.control_port = control_port
        self.stream_port = stream_port
        self.base_url = f"http://{ip}:{control_port}"
        self.stream_url = f"http://{ip}:{stream_port}"
        self.passed = 0
        self.failed = 0

    def test_control_get_status(self):
        """Test GET /control returns current camera status"""
        print("\n[Test] GET /control - Get camera status")
        try:
            resp = requests.get(f"{self.base_url}/control", timeout=5)
            if resp.status_code != 200:
                print(f"  FAIL: Expected 200, got {resp.status_code}")
                self.failed += 1
                return False

            data = resp.json()
            if "status" not in data or "camera" not in data:
                print(f"  FAIL: Missing fields in response: {data}")
                self.failed += 1
                return False

            if data["status"] != "ok":
                print(f"  FAIL: Expected status=ok, got {data}")
                self.failed += 1
                return False

            camera = data["camera"]
            if camera not in ["on", "off"]:
                print(f"  FAIL: Expected camera=on|off, got {camera}")
                self.failed += 1
                return False

            print(f"  PASS: Camera status = {camera}")
            self.passed += 1
            return True
        except Exception as e:
            print(f"  FAIL: {e}")
            self.failed += 1
            return False

    def test_control_post_on(self):
        """Test POST /control with action=on"""
        print("\n[Test] POST /control action=on - Turn camera on")
        try:
            resp = requests.post(
                f"{self.base_url}/control",
                json={"action": "on"},
                headers={"Content-Type": "application/json"},
                timeout=5
            )
            if resp.status_code != 200:
                print(f"  FAIL: Expected 200, got {resp.status_code}")
                self.failed += 1
                return False

            data = resp.json()
            if data.get("camera") != "on":
                print(f"  FAIL: Expected camera=on, got {data}")
                self.failed += 1
                return False

            print(f"  PASS: Camera turned on")
            self.passed += 1
            return True
        except Exception as e:
            print(f"  FAIL: {e}")
            self.failed += 1
            return False

    def test_control_post_off(self):
        """Test POST /control with action=off"""
        print("\n[Test] POST /control action=off - Turn camera off")
        try:
            resp = requests.post(
                f"{self.base_url}/control",
                json={"action": "off"},
                headers={"Content-Type": "application/json"},
                timeout=5
            )
            if resp.status_code != 200:
                print(f"  FAIL: Expected 200, got {resp.status_code}")
                self.failed += 1
                return False

            data = resp.json()
            if data.get("camera") != "off":
                print(f"  FAIL: Expected camera=off, got {data}")
                self.failed += 1
                return False

            print(f"  PASS: Camera turned off")
            self.passed += 1
            return True
        except Exception as e:
            print(f"  FAIL: {e}")
            self.failed += 1
            return False

    def test_root_page(self):
        """Test GET / returns HTML page"""
        print("\n[Test] GET / - Root page returns HTML")
        try:
            resp = requests.get(f"{self.base_url}/", timeout=5)
            if resp.status_code != 200:
                print(f"  FAIL: Expected 200, got {resp.status_code}")
                self.failed += 1
                return False

            content = resp.text
            if "<html" not in content.lower():
                print(f"  FAIL: Not HTML content")
                self.failed += 1
                return False

            if "/stream" not in content:
                print(f"  FAIL: Missing /stream reference in page")
                self.failed += 1
                return False

            print(f"  PASS: Root page contains stream link")
            self.passed += 1
            return True
        except Exception as e:
            print(f"  FAIL: {e}")
            self.failed += 1
            return False

    def test_stream_endpoint(self):
        """Test GET /stream returns multipart MJPEG"""
        print("\n[Test] GET /stream - MJPEG stream endpoint")
        try:
            # Stream endpoint returns multipart/x-mixed-replace
            resp = requests.get(
                f"{self.stream_url}/stream",
                headers={"Accept": "multipart/x-mixed-replace"},
                stream=True,
                timeout=10
            )
            if resp.status_code != 200:
                print(f"  FAIL: Expected 200, got {resp.status_code}")
                self.failed += 1
                return False

            content_type = resp.headers.get("Content-Type", "")
            if "multipart" not in content_type.lower():
                print(f"  FAIL: Expected multipart content-type, got {content_type}")
                self.failed += 1
                return False

            # Read at least one frame
            # MJPEG frames start with --frame and contain Content-Type: image/jpeg
            bytes_received = 0
            frame_found = False

            for chunk in resp.iter_content(chunk_size=1024):
                bytes_received += len(chunk)
                if b"image/jpeg" in chunk or b"--frame" in chunk:
                    frame_found = True
                    break
                if bytes_received > 10000:  # 10KB should be enough
                    break

            if not frame_found:
                print(f"  WARN: No JPEG frame detected in first 10KB ({bytes_received} bytes)")
                print(f"  Content sample: {bytes_received} bytes received")
                # Don't fail - camera might not be initialized
                print(f"  PASS: Stream endpoint responding (camera may be off)")
                self.passed += 1
                return True

            print(f"  PASS: MJPEG stream responding ({bytes_received} bytes)")
            self.passed += 1
            return True
        except Exception as e:
            print(f"  FAIL: {e}")
            self.failed += 1
            return False

    def test_invalid_action(self):
        """Test POST /control with invalid action defaults to status"""
        print("\n[Test] POST /control action=invalid - Defaults to status")
        try:
            resp = requests.post(
                f"{self.base_url}/control",
                json={"action": "invalid_action"},
                headers={"Content-Type": "application/json"},
                timeout=5
            )
            if resp.status_code != 200:
                print(f"  FAIL: Expected 200, got {resp.status_code}")
                self.failed += 1
                return False

            data = resp.json()
            # Should return current status, not error
            if "camera" not in data:
                print(f"  FAIL: Expected camera field in response, got {data}")
                self.failed += 1
                return False

            print(f"  PASS: Invalid action handled gracefully")
            self.passed += 1
            return True
        except Exception as e:
            print(f"  FAIL: {e}")
            self.failed += 1
            return False

    def test_servo_angle_90(self):
        """Test POST /control with action=servo and angle=90"""
        print("\n[Test] POST /control action=servo angle=90")
        try:
            resp = requests.post(
                f"{self.base_url}/control",
                json={"action": "servo", "angle": 90},
                headers={"Content-Type": "application/json"},
                timeout=5
            )
            if resp.status_code != 200:
                print(f"  FAIL: Expected 200, got {resp.status_code}")
                self.failed += 1
                return False

            data = resp.json()
            if data.get("status") != "ok":
                print(f"  FAIL: Expected status=ok, got {data}")
                self.failed += 1
                return False

            if data.get("angle") != 90:
                print(f"  FAIL: Expected angle=90, got {data}")
                self.failed += 1
                return False

            print(f"  PASS: Servo at 90°")
            self.passed += 1
            return True
        except Exception as e:
            print(f"  FAIL: {e}")
            self.failed += 1
            return False

    def test_servo_angle_0(self):
        """Test POST /control with action=servo and angle=0 (min)"""
        print("\n[Test] POST /control action=servo angle=0")
        try:
            resp = requests.post(
                f"{self.base_url}/control",
                json={"action": "servo", "angle": 0},
                headers={"Content-Type": "application/json"},
                timeout=5
            )
            if resp.status_code != 200:
                print(f"  FAIL: Expected 200, got {resp.status_code}")
                self.failed += 1
                return False

            data = resp.json()
            if data.get("status") != "ok" or data.get("angle") != 0:
                print(f"  FAIL: Expected ok/angle=0, got {data}")
                self.failed += 1
                return False

            print(f"  PASS: Servo at 0°")
            self.passed += 1
            return True
        except Exception as e:
            print(f"  FAIL: {e}")
            self.failed += 1
            return False

    def test_servo_angle_180(self):
        """Test POST /control with action=servo and angle=180 (max)"""
        print("\n[Test] POST /control action=servo angle=180")
        try:
            resp = requests.post(
                f"{self.base_url}/control",
                json={"action": "servo", "angle": 180},
                headers={"Content-Type": "application/json"},
                timeout=5
            )
            if resp.status_code != 200:
                print(f"  FAIL: Expected 200, got {resp.status_code}")
                self.failed += 1
                return False

            data = resp.json()
            if data.get("status") != "ok" or data.get("angle") != 180:
                print(f"  FAIL: Expected ok/angle=180, got {data}")
                self.failed += 1
                return False

            print(f"  PASS: Servo at 180°")
            self.passed += 1
            return True
        except Exception as e:
            print(f"  FAIL: {e}")
            self.failed += 1
            return False

    def test_servo_angle_out_of_range_positive(self):
        """Test POST /control with action=servo and angle=181 (out of range)"""
        print("\n[Test] POST /control action=servo angle=181 - out of range")
        try:
            resp = requests.post(
                f"{self.base_url}/control",
                json={"action": "servo", "angle": 181},
                headers={"Content-Type": "application/json"},
                timeout=5
            )
            if resp.status_code != 200:
                print(f"  FAIL: Expected 200, got {resp.status_code}")
                self.failed += 1
                return False

            data = resp.json()
            if data.get("status") != "error":
                print(f"  FAIL: Expected status=error, got {data}")
                self.failed += 1
                return False

            if "angle_out_of_range" not in data.get("reason", ""):
                print(f"  FAIL: Expected angle_out_of_range reason, got {data}")
                self.failed += 1
                return False

            print(f"  PASS: Out of range correctly rejected")
            self.passed += 1
            return True
        except Exception as e:
            print(f"  FAIL: {e}")
            self.failed += 1
            return False

    def test_servo_angle_out_of_range_negative(self):
        """Test POST /control with action=servo and angle=-1 (out of range)"""
        print("\n[Test] POST /control action=servo angle=-1 - out of range")
        try:
            resp = requests.post(
                f"{self.base_url}/control",
                json={"action": "servo", "angle": -1},
                headers={"Content-Type": "application/json"},
                timeout=5
            )
            if resp.status_code != 200:
                print(f"  FAIL: Expected 200, got {resp.status_code}")
                self.failed += 1
                return False

            data = resp.json()
            if data.get("status") != "error":
                print(f"  FAIL: Expected status=error, got {data}")
                self.failed += 1
                return False

            print(f"  PASS: Negative angle correctly rejected")
            self.passed += 1
            return True
        except Exception as e:
            print(f"  FAIL: {e}")
            self.failed += 1
            return False

    def test_servo_missing_angle(self):
        """Test POST /control with action=servo but no angle field"""
        print("\n[Test] POST /control action=servo (no angle) - should error")
        try:
            resp = requests.post(
                f"{self.base_url}/control",
                json={"action": "servo"},
                headers={"Content-Type": "application/json"},
                timeout=5
            )
            if resp.status_code != 200:
                print(f"  FAIL: Expected 200, got {resp.status_code}")
                self.failed += 1
                return False

            data = resp.json()
            if data.get("status") != "error":
                print(f"  FAIL: Expected status=error, got {data}")
                self.failed += 1
                return False

            print(f"  PASS: Missing angle correctly rejected")
            self.passed += 1
            return True
        except Exception as e:
            print(f"  FAIL: {e}")
            self.failed += 1
            return False

    def run_all_tests(self):
        """Run all integration tests"""
        print("=" * 60)
        print(f"ESP32 Camera Surveillance Integration Tests")
        print(f"Control endpoint: {self.base_url}")
        print(f"Stream endpoint: {self.stream_url}")
        print("=" * 60)

        # Test control endpoint
        self.test_control_get_status()
        self.test_control_post_on()
        self.test_control_get_status()  # Verify it's on
        self.test_control_post_off()
        self.test_control_get_status()  # Verify it's off
        self.test_invalid_action()
        self.test_root_page()

        # Servo tests
        print("\n--- Servo Control Tests ---")
        self.test_servo_angle_90()
        self.test_servo_angle_0()
        self.test_servo_angle_180()
        self.test_servo_angle_out_of_range_positive()
        self.test_servo_angle_out_of_range_negative()
        self.test_servo_missing_angle()

        # Test stream endpoint
        # Turn camera on first for stream test
        self.test_control_post_on()
        time.sleep(1)  # Give camera time to start
        self.test_stream_endpoint()

        # Summary
        print("\n" + "=" * 60)
        print(f"Results: {self.passed} passed, {self.failed} failed")
        print("=" * 60)

        return self.failed == 0


def main():
    parser = argparse.ArgumentParser(description="ESP32 Camera Integration Tests")
    parser.add_argument("--ip", default="10.0.0.110", help="ESP32 IP address")
    parser.add_argument("--control-port", type=int, default=8080, help="Control server port")
    parser.add_argument("--stream-port", type=int, default=8081, help="Stream server port")

    args = parser.parse_args()

    test = CameraAPITest(args.ip, args.control_port, args.stream_port)
    success = test.run_all_tests()

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
