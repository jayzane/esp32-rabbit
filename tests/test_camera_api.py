"""
ESP32 Camera Surveillance Integration Tests

Requires ESP32 running with firmware flashed.
Tests the HTTP control and MJPEG stream endpoints.

Usage:
    pytest tests/test_camera_api.py -v
    pytest tests/test_camera_api.py -v --ip 10.0.0.110 --control-port 8080 --stream-port 8081
"""

import pytest


def test_control_get_status(base_url, session):
    """Test GET /control returns current camera status"""
    resp = session.get(f"{base_url}/control", timeout=5)
    assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"

    data = resp.json()
    assert "status" in data, f"Missing 'status' field: {data}"
    assert "camera" in data, f"Missing 'camera' field: {data}"
    assert data["status"] == "ok", f"Expected status=ok, got {data}"

    camera = data["camera"]
    assert camera in ["on", "off"], f"Expected camera=on|off, got {camera}"


def test_control_post_on(base_url, session):
    """Test POST /control with action=on"""
    resp = session.post(
        f"{base_url}/control",
        json={"action": "on"},
        headers={"Content-Type": "application/json"},
        timeout=5
    )
    assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"

    data = resp.json()
    assert data.get("camera") == "on", f"Expected camera=on, got {data}"


def test_control_post_off(base_url, session):
    """Test POST /control with action=off"""
    resp = session.post(
        f"{base_url}/control",
        json={"action": "off"},
        headers={"Content-Type": "application/json"},
        timeout=5
    )
    assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"

    data = resp.json()
    assert data.get("camera") == "off", f"Expected camera=off, got {data}"


def test_root_page(base_url, session):
    """Test GET / returns HTML page"""
    resp = session.get(f"{base_url}/", timeout=5)
    assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"

    content = resp.text
    assert "<html" in content.lower(), "Not HTML content"

    assert "/stream" in content, "Missing /stream reference in page"


def test_stream_endpoint(stream_url, base_url, session):
    """Test GET /stream returns multipart MJPEG"""
    # Turn camera on first
    session.post(
        f"{base_url}/control",
        json={"action": "on"},
        headers={"Content-Type": "application/json"},
        timeout=5
    )

    resp = session.get(
        f"{stream_url}/stream",
        headers={"Accept": "multipart/x-mixed-replace"},
        stream=True,
        timeout=10
    )
    assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"

    content_type = resp.headers.get("Content-Type", "")
    assert "multipart" in content_type.lower(), f"Expected multipart content-type, got {content_type}"

    bytes_received = 0

    for chunk in resp.iter_content(chunk_size=1024):
        bytes_received += len(chunk)
        if bytes_received > 10000:
            break

    # Don't fail if no frame - camera might not be initialized
    # Just verify the stream endpoint is responding
    assert bytes_received > 0, "Stream endpoint did not respond"


def test_invalid_action(base_url, session):
    """Test POST /control with invalid action returns current status"""
    resp = session.post(
        f"{base_url}/control",
        json={"action": "invalid_action"},
        headers={"Content-Type": "application/json"},
        timeout=5
    )
    assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"

    data = resp.json()
    # Should return current status, not error
    assert "camera" in data, f"Expected camera field in response, got {data}"


def test_camera_on_off_sequence(base_url, session):
    """Test turning camera on then off"""
    # Turn on
    resp = session.post(
        f"{base_url}/control",
        json={"action": "on"},
        headers={"Content-Type": "application/json"},
        timeout=5
    )
    assert resp.status_code == 200
    assert resp.json().get("camera") == "on"

    # Verify on
    resp = session.get(f"{base_url}/control", timeout=5)
    assert resp.json().get("camera") == "on"

    # Turn off
    resp = session.post(
        f"{base_url}/control",
        json={"action": "off"},
        headers={"Content-Type": "application/json"},
        timeout=5
    )
    assert resp.status_code == 200
    assert resp.json().get("camera") == "off"

    # Verify off
    resp = session.get(f"{base_url}/control", timeout=5)
    assert resp.json().get("camera") == "off"
