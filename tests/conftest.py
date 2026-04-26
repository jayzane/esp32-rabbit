"""
pytest configuration for ESP32 Camera Surveillance Integration Tests

Requires ESP32 running with firmware flashed.
"""

import pytest


def pytest_addoption(parser):
    parser.addoption("--ip", action="store", default="10.0.0.110", help="ESP32 IP address")
    parser.addoption("--control-port", action="store", type=int, default=8080, help="Control server port")
    parser.addoption("--stream-port", action="store", type=int, default=8081, help="Stream server port")


@pytest.fixture
def ip(request):
    return request.config.getoption("--ip")


@pytest.fixture
def control_port(request):
    return request.config.getoption("--control-port")


@pytest.fixture
def stream_port(request):
    return request.config.getoption("--stream-port")


@pytest.fixture
def base_url(ip, control_port):
    return f"http://{ip}:{control_port}"


@pytest.fixture
def stream_url(ip, stream_port):
    return f"http://{ip}:{stream_port}"


@pytest.fixture
def session():
    import requests
    return requests.Session()
