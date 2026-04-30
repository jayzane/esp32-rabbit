# Graph Report - .  (2026-04-29)

## Corpus Check
- 155 files ¡¤ ~178,609 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 209 nodes ¡¤ 271 edges ¡¤ 19 communities detected
- Extraction: 85% EXTRACTED ¡¤ 15% INFERRED ¡¤ 0% AMBIGUOUS ¡¤ INFERRED: 41 edges (avg confidence: 0.67)
- Token cost: 0 input ¡¤ 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Community 0|Community 0]]
- [[_COMMUNITY_Community 1|Community 1]]
- [[_COMMUNITY_Community 2|Community 2]]
- [[_COMMUNITY_Community 3|Community 3]]
- [[_COMMUNITY_Community 4|Community 4]]
- [[_COMMUNITY_Community 5|Community 5]]
- [[_COMMUNITY_Community 6|Community 6]]
- [[_COMMUNITY_Community 7|Community 7]]
- [[_COMMUNITY_Community 8|Community 8]]
- [[_COMMUNITY_Community 9|Community 9]]
- [[_COMMUNITY_Community 10|Community 10]]
- [[_COMMUNITY_Community 11|Community 11]]
- [[_COMMUNITY_Community 12|Community 12]]
- [[_COMMUNITY_Community 13|Community 13]]
- [[_COMMUNITY_Community 14|Community 14]]
- [[_COMMUNITY_Community 15|Community 15]]
- [[_COMMUNITY_Community 16|Community 16]]
- [[_COMMUNITY_Community 17|Community 17]]
- [[_COMMUNITY_Community 18|Community 18]]

## God Nodes (most connected - your core abstractions)
1. `app_main()` - 11 edges
2. `TestWSResponseFormat` - 10 edges
3. `app_main` - 9 edges
4. `TestWSCommandProtocol` - 8 edges
5. `camera_ctrl_enable()` - 7 edges
6. `ws_control_init` - 7 edges
7. `ws_control_callback` - 7 edges
8. `ws_command_callback()` - 7 edges
9. `servo_set_angle` - 6 edges
10. `camera_ctrl_disable()` - 5 edges

## Surprising Connections (you probably didn't know these)
- `app_main` --is_part_of--> `Camera Surveillance System Architecture`  [INFERRED]
  main/app_main.c ¡ú graphify-out/GRAPH_REPORT.md
- `app_main()` --calls--> `camera_ctrl_init()`  [INFERRED]
  main\app_main.c ¡ú main\camera_ctrl.c
- `app_main()` --calls--> `shared_mem_init()`  [INFERRED]
  main\app_main.c ¡ú main\shared_mem.c
- `servo_set_angle` --controls_via_HTTP--> `ESP32 Monitor Frontend Design`  [EXTRACTED]
  main/servo.c ¡ú docs/superpowers/specs/2026-04-26-esp32-monitor-frontend-design.md
- `control_post_handler` --returns_error--> `angle_out_of_range error`  [EXTRACTED]
  main/ws_control.c ¡ú docs/superpowers/specs/2026-04-26-servo-control-design.md

## Communities

### Community 0 - "Community 0"
Cohesion: 0.1
Nodes (20): app_main(), configure_static_ip(), wait_for_wifi_connection(), camera_driver, camera_task, camera_task_start(), esp_camera, esp_event (+12 more)

### Community 1 - "Community 1"
Cohesion: 0.11
Nodes (14): ws_control_callback(), camera_ctrl_is_on(), camera_server_start(), esp_http_server, camera_server.c, servo.c, ws_control.c, angle_to_duty() (+6 more)

### Community 2 - "Community 2"
Cohesion: 0.13
Nodes (19): Camera Surveillance System Architecture, camera_ctrl, camera_ctrl_deinit(), camera_ctrl_disable(), camera_ctrl_enable(), camera_ctrl_init(), camera_driver_deinit(), camera_driver_get_frame() (+11 more)

### Community 3 - "Community 3"
Cohesion: 0.13
Nodes (17): ws_command_callback(), camera_ctrl_capture(), esp_random, ip4_addr, netdb, sockets, ws_client, base64_encode() (+9 more)

### Community 4 - "Community 4"
Cohesion: 0.11
Nodes (20): ESP32-WROVER-DEV, GPIO13, LEDC PWM, OV2640, SERVO_FREQUENCY_HZ, SG90 Servo, WiFi STA Mode, app_main (+12 more)

### Community 5 - "Community 5"
Cohesion: 0.11
Nodes (10): capture_failed error response, unknown_command error response, Test ESP32 response format, All responses must include seq field, camera_on ok response format, servo ok response format, capture ok response with frame_size, init_failed error response (+2 more)

### Community 6 - "Community 6"
Cohesion: 0.11
Nodes (12): json, pytest, struct, pytest tests/test_ws_commands.py Requires: pip install pytest, Test JPEG binary frame format over WebSocket, Binary frame: 4-byte little-endian length prefix + JPEG data, JPEG data must start with SOI marker 0xFF 0xD8, JPEG_BUF_SIZE = 128KB = 131072 bytes (+4 more)

### Community 7 - "Community 7"
Cohesion: 0.14
Nodes (15): SERVO_LEDC_CHANNEL, SERVO_LEDC_PERIOD, SERVO_MAX_ANGLE, SERVO_MIN_ANGLE, Servo Control Interface Design, Servo Control Implementation Plan, WS_ACTION_OFF, WS_ACTION_ON (+7 more)

### Community 8 - "Community 8"
Cohesion: 0.14
Nodes (8): Test ESP32 WS client command protocol, camera_on command JSON parsing, camera_off command JSON parsing, servo command with angle parameter, capture command parsing, stream_start command parsing, stream_stop command parsing, TestWSCommandProtocol

### Community 9 - "Community 9"
Cohesion: 0.22
Nodes (7): esp_heap_caps, freertos, queue, semphr, shared_mem_init(), stddef, stdint

### Community 10 - "Community 10"
Cohesion: 0.32
Nodes (8): ESP32 Camera Surveillance Integration Tests, ESP32 Static IP Configuration, pytest fixtures (conftest.py), test_camera_on_off_sequence, test_control_get_status, test_control_post_off, test_control_post_on, test_stream_endpoint

### Community 11 - "Community 11"
Cohesion: 0.25
Nodes (8): 8px Spacing System, Dark Theme Design System, ESP32 Monitor Frontend Design, ESP32 Monitor Frontend Implementation Plan, JetBrains Mono Font, MJPEG Stream, Three Phase Deployment, camera_server_start

### Community 12 - "Community 12"
Cohesion: 1.0
Nodes (1): camera_driver.h

### Community 13 - "Community 13"
Cohesion: 1.0
Nodes (1): camera_server.h

### Community 14 - "Community 14"
Cohesion: 1.0
Nodes (1): servo.h

### Community 15 - "Community 15"
Cohesion: 1.0
Nodes (1): ws_control.h

### Community 16 - "Community 16"
Cohesion: 1.0
Nodes (1): servo_deinit

### Community 17 - "Community 17"
Cohesion: 1.0
Nodes (1): SERVO_GPIO

### Community 18 - "Community 18"
Cohesion: 1.0
Nodes (1): test_invalid_action

## Knowledge Gaps
- **50 isolated node(s):** `servo_get_angle`, `servo_deinit`, `SERVO_GPIO`, `SERVO_LEDC_CHANNEL`, `SERVO_FREQUENCY_HZ` (+45 more)
  These have ¡Ü1 connection - possible missing edges or undocumented components.
- **Thin community `Community 12`** (1 nodes): `camera_driver.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 13`** (1 nodes): `camera_server.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 14`** (1 nodes): `servo.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 15`** (1 nodes): `ws_control.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 16`** (1 nodes): `servo_deinit`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 17`** (1 nodes): `SERVO_GPIO`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 18`** (1 nodes): `test_invalid_action`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.