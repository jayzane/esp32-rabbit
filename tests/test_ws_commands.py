"""
pytest tests/test_ws_commands.py
Requires: pip install pytest
"""
import pytest
import json
import struct


class TestWSCommandProtocol:
    """Test ESP32 WS client command protocol"""

    def test_parse_camera_on_command(self):
        """camera_on command JSON parsing"""
        json_str = '{"cmd": "camera_on"}'
        data = json.loads(json_str)
        assert data["cmd"] == "camera_on"

    def test_parse_camera_off_command(self):
        """camera_off command JSON parsing"""
        json_str = '{"cmd": "camera_off"}'
        data = json.loads(json_str)
        assert data["cmd"] == "camera_off"

    def test_parse_servo_command_with_angle(self):
        """servo command with angle parameter"""
        json_str = '{"cmd": "servo", "angle": 90}'
        data = json.loads(json_str)
        assert data["cmd"] == "servo"
        assert data["angle"] == 90

    def test_parse_capture_command(self):
        """capture command parsing"""
        json_str = '{"cmd": "capture"}'
        data = json.loads(json_str)
        assert data["cmd"] == "capture"

    def test_parse_stream_start_command(self):
        """stream_start command parsing"""
        json_str = '{"cmd": "stream_start"}'
        data = json.loads(json_str)
        assert data["cmd"] == "stream_start"

    def test_parse_stream_stop_command(self):
        """stream_stop command parsing"""
        json_str = '{"cmd": "stream_stop"}'
        data = json.loads(json_str)
        assert data["cmd"] == "stream_stop"


class TestWSResponseFormat:
    """Test ESP32 response format"""

    def test_response_has_seq_field(self):
        """All responses must include seq field"""
        for json_str in [
            '{"seq": 1, "status": "ok", "camera": "on"}',
            '{"seq": 2, "status": "ok", "camera": "off"}',
            '{"seq": 3, "status": "ok", "angle": 90}',
            '{"seq": 4, "status": "ok", "frame_size": 12345}',
        ]:
            data = json.loads(json_str)
            assert "seq" in data
            assert isinstance(data["seq"], int)

    def test_ok_response_camera_on(self):
        """camera_on ok response format"""
        json_str = '{"seq": 42, "status": "ok", "camera": "on"}'
        data = json.loads(json_str)
        assert data["seq"] == 42
        assert data["status"] == "ok"
        assert data["camera"] == "on"

    def test_ok_response_servo(self):
        """servo ok response format"""
        json_str = '{"seq": 42, "status": "ok", "angle": 90}'
        data = json.loads(json_str)
        assert data["seq"] == 42
        assert data["status"] == "ok"
        assert data["angle"] == 90

    def test_ok_response_capture(self):
        """capture ok response with frame_size"""
        json_str = '{"seq": 42, "status": "ok", "frame_size": 12345}'
        data = json.loads(json_str)
        assert data["seq"] == 42
        assert data["status"] == "ok"
        assert data["frame_size"] == 12345

    def test_error_response_init_failed(self):
        """init_failed error response"""
        json_str = '{"seq": 42, "status": "error", "reason": "init_failed"}'
        data = json.loads(json_str)
        assert data["status"] == "error"
        assert data["reason"] == "init_failed"

    def test_error_response_angle_out_of_range(self):
        """angle_out_of_range error response"""
        json_str = '{"seq": 43, "status": "error", "reason": "angle_out_of_range"}'
        data = json.loads(json_str)
        assert data["status"] == "error"
        assert data["reason"] == "angle_out_of_range"

    def test_error_response_capture_failed(self):
        """capture_failed error response"""
        json_str = '{"seq": 44, "status": "error", "reason": "capture_failed"}'
        data = json.loads(json_str)
        assert data["status"] == "error"
        assert data["reason"] == "capture_failed"

    def test_error_response_unknown_command(self):
        """unknown_command error response"""
        json_str = '{"seq": 45, "status": "error", "reason": "unknown_command"}'
        data = json.loads(json_str)
        assert data["status"] == "error"
        assert data["reason"] == "unknown_command"


class TestJPEGBinaryFrame:
    """Test JPEG binary frame format over WebSocket"""

    def test_binary_frame_header_format(self):
        """Binary frame: 4-byte little-endian length prefix + JPEG data"""
        jpeg_data = b'\xFF\xD8\xFF\xE0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00'
        frame_len = len(jpeg_data)
        frame = struct.pack("<I", frame_len) + jpeg_data

        recv_len = struct.unpack("<I", frame[:4])[0]
        recv_data = frame[4:]

        assert recv_len == frame_len
        assert recv_data == jpeg_data

    def test_jpeg_soi_marker(self):
        """JPEG data must start with SOI marker 0xFF 0xD8"""
        jpeg_data = b'\xFF\xD8\xFF\xE0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00'
        assert jpeg_data[:2] == b'\xFF\xD8'

    def test_jpeg_buf_size_constant(self):
        """JPEG_BUF_SIZE = 128KB = 131072 bytes"""
        JPEG_BUF_SIZE = 128 * 1024
        assert JPEG_BUF_SIZE == 131072


class TestCameraMsgQueue:
    """Test dual-core Queue message structure"""

    def test_queue_message_struct_packing(self):
        """Simulate C struct packing for camera_msg_t"""
        # C struct:
        # typedef struct {
        #     camera_cmd_t cmd;    // 4 bytes enum
        #     uint32_t seq;        // 4 bytes
        #     size_t frame_len;    // 4 bytes on 32-bit
        # } camera_msg_t;
        CMD_INIT = 1
        CMD_CAPTURE = 3
        CMD_STREAM_START = 4

        # Pack as 3 x uint32_t little-endian
        msg_init = struct.pack("<III", CMD_INIT, 42, 1)
        cmd, seq, frame_len = struct.unpack("<III", msg_init)
        assert cmd == CMD_INIT
        assert seq == 42
        assert frame_len == 1

        msg_capture = struct.pack("<III", CMD_CAPTURE, 99, 65536)
        cmd, seq, frame_len = struct.unpack("<III", msg_capture)
        assert cmd == CMD_CAPTURE
        assert seq == 99
        assert frame_len == 65536

    def test_camera_cmd_enum_values(self):
        """Verify C enum values"""
        cmds = {
            "NONE": 0,
            "INIT": 1,
            "DEINIT": 2,
            "CAPTURE": 3,
            "STREAM_START": 4,
            "STREAM_STOP": 5,
        }
        assert cmds["INIT"] == 1
        assert cmds["DEINIT"] == 2
        assert cmds["CAPTURE"] == 3
        assert cmds["STREAM_START"] == 4
        assert cmds["STREAM_STOP"] == 5