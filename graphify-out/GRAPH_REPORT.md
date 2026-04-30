# Graph Report - E:\projects\esp32-rabbit  (2026-04-30)

## Corpus Check
- 124 files · ~175,589 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 994 nodes · 1988 edges · 75 communities detected
- Extraction: 85% EXTRACTED · 15% INFERRED · 0% AMBIGUOUS · INFERRED: 304 edges (avg confidence: 0.78)
- Token cost: 0 input · 0 output

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
- [[_COMMUNITY_Community 19|Community 19]]
- [[_COMMUNITY_Community 20|Community 20]]
- [[_COMMUNITY_Community 21|Community 21]]
- [[_COMMUNITY_Community 22|Community 22]]
- [[_COMMUNITY_Community 23|Community 23]]
- [[_COMMUNITY_Community 24|Community 24]]
- [[_COMMUNITY_Community 25|Community 25]]
- [[_COMMUNITY_Community 26|Community 26]]
- [[_COMMUNITY_Community 27|Community 27]]
- [[_COMMUNITY_Community 28|Community 28]]
- [[_COMMUNITY_Community 29|Community 29]]
- [[_COMMUNITY_Community 30|Community 30]]
- [[_COMMUNITY_Community 31|Community 31]]
- [[_COMMUNITY_Community 32|Community 32]]
- [[_COMMUNITY_Community 33|Community 33]]
- [[_COMMUNITY_Community 34|Community 34]]
- [[_COMMUNITY_Community 35|Community 35]]
- [[_COMMUNITY_Community 36|Community 36]]
- [[_COMMUNITY_Community 37|Community 37]]
- [[_COMMUNITY_Community 38|Community 38]]
- [[_COMMUNITY_Community 39|Community 39]]
- [[_COMMUNITY_Community 40|Community 40]]
- [[_COMMUNITY_Community 41|Community 41]]
- [[_COMMUNITY_Community 42|Community 42]]
- [[_COMMUNITY_Community 43|Community 43]]
- [[_COMMUNITY_Community 44|Community 44]]
- [[_COMMUNITY_Community 45|Community 45]]
- [[_COMMUNITY_Community 46|Community 46]]
- [[_COMMUNITY_Community 47|Community 47]]
- [[_COMMUNITY_Community 48|Community 48]]
- [[_COMMUNITY_Community 49|Community 49]]
- [[_COMMUNITY_Community 50|Community 50]]
- [[_COMMUNITY_Community 51|Community 51]]
- [[_COMMUNITY_Community 52|Community 52]]
- [[_COMMUNITY_Community 53|Community 53]]
- [[_COMMUNITY_Community 54|Community 54]]
- [[_COMMUNITY_Community 55|Community 55]]
- [[_COMMUNITY_Community 56|Community 56]]
- [[_COMMUNITY_Community 57|Community 57]]
- [[_COMMUNITY_Community 58|Community 58]]
- [[_COMMUNITY_Community 59|Community 59]]
- [[_COMMUNITY_Community 60|Community 60]]
- [[_COMMUNITY_Community 61|Community 61]]
- [[_COMMUNITY_Community 62|Community 62]]
- [[_COMMUNITY_Community 63|Community 63]]
- [[_COMMUNITY_Community 64|Community 64]]
- [[_COMMUNITY_Community 65|Community 65]]
- [[_COMMUNITY_Community 66|Community 66]]
- [[_COMMUNITY_Community 67|Community 67]]
- [[_COMMUNITY_Community 68|Community 68]]
- [[_COMMUNITY_Community 69|Community 69]]
- [[_COMMUNITY_Community 70|Community 70]]
- [[_COMMUNITY_Community 71|Community 71]]
- [[_COMMUNITY_Community 72|Community 72]]
- [[_COMMUNITY_Community 73|Community 73]]
- [[_COMMUNITY_Community 74|Community 74]]

## God Nodes (most connected - your core abstractions)
1. `SCCB_Write()` - 48 edges
2. `write_reg_bits()` - 47 edges
3. `SCCB_Read()` - 44 edges
4. `esp_camera_load_from_nvs()` - 26 edges
5. `write_reg()` - 26 edges
6. `camera_driver_init()` - 26 edges
7. `write_reg()` - 22 edges
8. `write_reg()` - 22 edges
9. `write_reg()` - 19 edges
10. `write_reg()` - 19 edges

## Surprising Connections (you probably didn't know these)
- `printf_img_base64()` --calls--> `fmt2jpg()`  [INFERRED]
  components\esp32-camera\test\test_camera.c → components\esp32-camera\conversions\to_jpg.cpp
- `esp_camera_load_from_nvs()` --calls--> `set_special_effect()`  [INFERRED]
  components\esp32-camera\driver\esp_camera.c → components\esp32-camera\sensors\sc031gs.c
- `init_status()` --calls--> `SCCB_Read()`  [INFERRED]
  components\esp32-camera\sensors\bf20a6.c → components\esp32-camera\driver\sccb.c
- `esp32_camera_bf20a6_detect()` --calls--> `SCCB_Read()`  [INFERRED]
  components\esp32-camera\sensors\bf20a6.c → components\esp32-camera\driver\sccb.c
- `esp32_camera_sc030iot_detect()` --calls--> `SCCB_Read()`  [INFERRED]
  components\esp32-camera\sensors\sc030iot.c → components\esp32-camera\driver\sccb.c

## Communities

### Community 0 - "Community 0"
Cohesion: 0.05
Nodes (90): write_reg_bits(), calc_sysclk(), check_reg_mask(), get_aec_value(), get_agc_gain(), get_denoise(), get_reg(), init_status() (+82 more)

### Community 1 - "Community 1"
Cohesion: 0.03
Nodes (73): Camera Surveillance System Architecture, GPIO13, LEDC PWM, SERVO_FREQUENCY_HZ, SERVO_LEDC_PERIOD, SERVO_MAX_ANGLE, SERVO_MIN_ANGLE, SG90 Servo (+65 more)

### Community 2 - "Community 2"
Cohesion: 0.05
Nodes (72): allocate_dma_descriptors(), cam_config(), cam_deinit(), cam_dma_config(), cam_drop_psram_cache(), cam_get_available_frames(), cam_get_next_frame(), cam_get_psram_mode() (+64 more)

### Community 3 - "Community 3"
Cohesion: 0.05
Nodes (67): esp32_camera_bf3005_detect(), get_reg(), get_reg_bits(), init_status(), reset(), set_agc_gain(), set_awb_gain_dsp(), set_brightness() (+59 more)

### Community 4 - "Community 4"
Cohesion: 0.07
Nodes (35): check_reg_mask(), esp32_camera_hm0360_detect(), get_reg(), init_status(), read_reg(), read_reg16(), reset(), set_brightness() (+27 more)

### Community 5 - "Community 5"
Cohesion: 0.04
Nodes (30): json, pytest, struct, pytest tests/test_ws_commands.py Requires: pip install pytest, capture_failed error response, Test ESP32 WS client command protocol, unknown_command error response, Test JPEG binary frame format over WebSocket (+22 more)

### Community 6 - "Community 6"
Cohesion: 0.06
Nodes (15): esp_camera, esp_heap_caps, queue, stdbool, stddef, stdint, callback_stream, convert_image() (+7 more)

### Community 7 - "Community 7"
Cohesion: 0.08
Nodes (35): check_reg_mask(), get_aec_value(), get_agc_gain(), get_denoise(), get_reg(), init_status(), read_reg(), read_reg16() (+27 more)

### Community 8 - "Community 8"
Cohesion: 0.08
Nodes (30): get_reg(), get_reg_bits(), init_status(), read_reg(), set_aec2(), set_aec_sensor(), set_aec_value(), set_agc_gain() (+22 more)

### Community 9 - "Community 9"
Cohesion: 0.11
Nodes (31): check_reg_mask(), get_denoise(), get_reg(), get_sharpness(), init_status(), read_reg(), read_reg16(), reset() (+23 more)

### Community 10 - "Community 10"
Cohesion: 0.09
Nodes (30): decode_image(), esp_camera_af_get_status(), esp_camera_af_init(), esp_camera_af_is_supported(), esp_camera_af_set_manual_position(), esp_camera_af_set_mode(), esp_camera_af_trigger(), esp_camera_af_wait() (+22 more)

### Community 11 - "Community 11"
Cohesion: 0.14
Nodes (36): clamp(), clear(), code_block(), code_coefficients_pass_two(), compute_huffman_table(), compute_quant_table(), DCT2D(), deinit() (+28 more)

### Community 12 - "Community 12"
Cohesion: 0.11
Nodes (30): esp_system, esp_jpeg_decode(), esp_jpeg_get_image_info(), jpeg_decode_out_cb(), jpeg_get_color_bytes(), jpeg_get_div_by_scale(), ldb_word(), tjpgd_decode_rgb565() (+22 more)

### Community 13 - "Community 13"
Cohesion: 0.16
Nodes (27): check_reg_mask(), esp32_camera_gc0308_detect(), get_reg(), init_status(), print_regs(), read_reg(), reset(), set_ae_level() (+19 more)

### Community 14 - "Community 14"
Cohesion: 0.1
Nodes (8): esp32_camera_sc030iot_detect(), get_reg(), reset(), set_framesize(), set_reg(), set_reg_bits(), set_regs(), set_window()

### Community 15 - "Community 15"
Cohesion: 0.16
Nodes (17): analog_gain(), esp32_camera_mega_ccm_detect(), exposure_line(), read_reg(), reset(), set_agc_mode(), set_brightness(), set_contrast() (+9 more)

### Community 16 - "Community 16"
Cohesion: 0.16
Nodes (18): check_reg_mask(), esp32_camera_bf20a6_detect(), get_reg(), init_status(), print_regs(), read_reg(), read_regs(), reset() (+10 more)

### Community 17 - "Community 17"
Cohesion: 0.21
Nodes (15): fetchStatus(), init(), sendServoAngle(), setCamera(), setupEventListeners(), showToast(), startPolling(), updateCameraButtons() (+7 more)

### Community 18 - "Community 18"
Cohesion: 0.25
Nodes (15): check_reg_mask(), get_reg(), init_status(), print_regs(), read_reg(), reset(), set_colorbar(), set_framesize() (+7 more)

### Community 19 - "Community 19"
Cohesion: 0.24
Nodes (14): check_reg_mask(), get_reg(), init_status(), print_regs(), read_reg(), reset(), set_framesize(), set_hmirror() (+6 more)

### Community 20 - "Community 20"
Cohesion: 0.29
Nodes (10): ov5640_af_firmware_load(), ov5640_af_get_status(), ov5640_af_init(), ov5640_af_set_mode(), ov5640_af_start(), ov5640_af_trigger(), ov5640_af_wait_ack_clear(), ov5640_af_wait_fw_idle() (+2 more)

### Community 21 - "Community 21"
Cohesion: 0.38
Nodes (7): ESP32 Camera Surveillance Integration Tests, pytest fixtures (conftest.py), test_camera_on_off_sequence, test_control_get_status, test_control_post_off, test_control_post_on, test_stream_endpoint

### Community 22 - "Community 22"
Cohesion: 0.33
Nodes (6): 8px Spacing System, Dark Theme Design System, ESP32 Monitor Frontend Design, ESP32 Monitor Frontend Implementation Plan, JetBrains Mono Font, Three Phase Deployment

### Community 23 - "Community 23"
Cohesion: 0.5
Nodes (4): jpg_to_rgb888_hex_c_array(), main(), Main function to convert a JPEG file to an RGB888 C-style hex array.      Instru, Convert a .jpg file to RGB888 hex data and format it as a C-style array.      Pa

### Community 24 - "Community 24"
Cohesion: 0.67
Nodes (3): Servo Control Interface Design, Servo Control Implementation Plan, angle_out_of_range error

### Community 25 - "Community 25"
Cohesion: 1.0
Nodes (0): 

### Community 26 - "Community 26"
Cohesion: 1.0
Nodes (0): 

### Community 27 - "Community 27"
Cohesion: 1.0
Nodes (0): 

### Community 28 - "Community 28"
Cohesion: 1.0
Nodes (0): 

### Community 29 - "Community 29"
Cohesion: 1.0
Nodes (0): 

### Community 30 - "Community 30"
Cohesion: 1.0
Nodes (0): 

### Community 31 - "Community 31"
Cohesion: 1.0
Nodes (0): 

### Community 32 - "Community 32"
Cohesion: 1.0
Nodes (0): 

### Community 33 - "Community 33"
Cohesion: 1.0
Nodes (0): 

### Community 34 - "Community 34"
Cohesion: 1.0
Nodes (0): 

### Community 35 - "Community 35"
Cohesion: 1.0
Nodes (0): 

### Community 36 - "Community 36"
Cohesion: 1.0
Nodes (0): 

### Community 37 - "Community 37"
Cohesion: 1.0
Nodes (0): 

### Community 38 - "Community 38"
Cohesion: 1.0
Nodes (0): 

### Community 39 - "Community 39"
Cohesion: 1.0
Nodes (0): 

### Community 40 - "Community 40"
Cohesion: 1.0
Nodes (0): 

### Community 41 - "Community 41"
Cohesion: 1.0
Nodes (0): 

### Community 42 - "Community 42"
Cohesion: 1.0
Nodes (0): 

### Community 43 - "Community 43"
Cohesion: 1.0
Nodes (0): 

### Community 44 - "Community 44"
Cohesion: 1.0
Nodes (0): 

### Community 45 - "Community 45"
Cohesion: 1.0
Nodes (0): 

### Community 46 - "Community 46"
Cohesion: 1.0
Nodes (0): 

### Community 47 - "Community 47"
Cohesion: 1.0
Nodes (0): 

### Community 48 - "Community 48"
Cohesion: 1.0
Nodes (0): 

### Community 49 - "Community 49"
Cohesion: 1.0
Nodes (0): 

### Community 50 - "Community 50"
Cohesion: 1.0
Nodes (0): 

### Community 51 - "Community 51"
Cohesion: 1.0
Nodes (0): 

### Community 52 - "Community 52"
Cohesion: 1.0
Nodes (0): 

### Community 53 - "Community 53"
Cohesion: 1.0
Nodes (0): 

### Community 54 - "Community 54"
Cohesion: 1.0
Nodes (0): 

### Community 55 - "Community 55"
Cohesion: 1.0
Nodes (0): 

### Community 56 - "Community 56"
Cohesion: 1.0
Nodes (0): 

### Community 57 - "Community 57"
Cohesion: 1.0
Nodes (0): 

### Community 58 - "Community 58"
Cohesion: 1.0
Nodes (0): 

### Community 59 - "Community 59"
Cohesion: 1.0
Nodes (1): servo_deinit

### Community 60 - "Community 60"
Cohesion: 1.0
Nodes (1): SERVO_GPIO

### Community 61 - "Community 61"
Cohesion: 1.0
Nodes (1): SERVO_LEDC_CHANNEL

### Community 62 - "Community 62"
Cohesion: 1.0
Nodes (0): 

### Community 63 - "Community 63"
Cohesion: 1.0
Nodes (0): 

### Community 64 - "Community 64"
Cohesion: 1.0
Nodes (0): 

### Community 65 - "Community 65"
Cohesion: 1.0
Nodes (0): 

### Community 66 - "Community 66"
Cohesion: 1.0
Nodes (0): 

### Community 67 - "Community 67"
Cohesion: 1.0
Nodes (0): 

### Community 68 - "Community 68"
Cohesion: 1.0
Nodes (0): 

### Community 69 - "Community 69"
Cohesion: 1.0
Nodes (0): 

### Community 70 - "Community 70"
Cohesion: 1.0
Nodes (0): 

### Community 71 - "Community 71"
Cohesion: 1.0
Nodes (1): test_invalid_action

### Community 72 - "Community 72"
Cohesion: 1.0
Nodes (1): camera_server.h

### Community 73 - "Community 73"
Cohesion: 1.0
Nodes (1): servo.h

### Community 74 - "Community 74"
Cohesion: 1.0
Nodes (1): ws_control.h

## Knowledge Gaps
- **41 isolated node(s):** `servo_get_angle`, `servo_deinit`, `SERVO_GPIO`, `SERVO_LEDC_CHANNEL`, `SERVO_FREQUENCY_HZ` (+36 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **Thin community `Community 25`** (2 nodes): `jpge.h`, `jpge()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 26`** (1 nodes): `esp_camera.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 27`** (1 nodes): `camera_pinout.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 28`** (1 nodes): `bf20a6.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 29`** (1 nodes): `bf20a6_regs.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 30`** (1 nodes): `bf3005.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 31`** (1 nodes): `bf3005_regs.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 32`** (1 nodes): `gc0308.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 33`** (1 nodes): `gc0308_regs.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 34`** (1 nodes): `gc032a.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 35`** (1 nodes): `gc032a_regs.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 36`** (1 nodes): `gc2145.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 37`** (1 nodes): `gc2145_regs.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 38`** (1 nodes): `hm0360.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 39`** (1 nodes): `hm0360_regs.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 40`** (1 nodes): `hm1055.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 41`** (1 nodes): `hm1055_regs.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 42`** (1 nodes): `mega_ccm.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 43`** (1 nodes): `mega_ccm_regs.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 44`** (1 nodes): `nt99141.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 45`** (1 nodes): `nt99141_regs.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 46`** (1 nodes): `ov2640.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 47`** (1 nodes): `ov2640_regs.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 48`** (1 nodes): `ov3660.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 49`** (1 nodes): `ov3660_regs.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 50`** (1 nodes): `ov5640.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 51`** (1 nodes): `ov5640_regs.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 52`** (1 nodes): `ov7670.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 53`** (1 nodes): `ov7670_regs.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 54`** (1 nodes): `ov7725.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 55`** (1 nodes): `ov7725_regs.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 56`** (1 nodes): `sc030iot.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 57`** (1 nodes): `sc031gs.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 58`** (1 nodes): `sc101iot.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 59`** (1 nodes): `servo_deinit`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 60`** (1 nodes): `SERVO_GPIO`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 61`** (1 nodes): `SERVO_LEDC_CHANNEL`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 62`** (1 nodes): `jpeg_default_huffman_table.c`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 63`** (1 nodes): `jpeg_decoder.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 64`** (1 nodes): `test_logo_jpg.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 65`** (1 nodes): `test_logo_rgb888.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 66`** (1 nodes): `test_usb_camera_2_jpg.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 67`** (1 nodes): `test_usb_camera_2_rgb888.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 68`** (1 nodes): `test_usb_camera_jpg.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 69`** (1 nodes): `test_usb_camera_rgb888.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 70`** (1 nodes): `tjpgdcnf.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 71`** (1 nodes): `test_invalid_action`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 72`** (1 nodes): `camera_server.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 73`** (1 nodes): `servo.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 74`** (1 nodes): `ws_control.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `esp_jpeg_decode()` connect `Community 12` to `Community 10`?**
  _High betweenness centrality (0.018) - this node is a cross-community bridge._
- **Why does `init()` connect `Community 17` to `Community 6`?**
  _High betweenness centrality (0.018) - this node is a cross-community bridge._
- **Are the 47 inferred relationships involving `SCCB_Write()` (e.g. with `write_reg()` and `set_reg_bits()`) actually correct?**
  _`SCCB_Write()` has 47 INFERRED edges - model-reasoned connections that need verification._
- **Are the 40 inferred relationships involving `write_reg_bits()` (e.g. with `set_hmirror()` and `set_vflip()`) actually correct?**
  _`write_reg_bits()` has 40 INFERRED edges - model-reasoned connections that need verification._
- **Are the 43 inferred relationships involving `SCCB_Read()` (e.g. with `read_reg()` and `set_reg_bits()`) actually correct?**
  _`SCCB_Read()` has 43 INFERRED edges - model-reasoned connections that need verification._
- **What connects `servo_get_angle`, `servo_deinit`, `SERVO_GPIO` to the rest of the system?**
  _41 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Community 0` be split into smaller, more focused modules?**
  _Cohesion score 0.05 - nodes in this community are weakly interconnected._