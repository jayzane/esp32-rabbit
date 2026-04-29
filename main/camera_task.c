#include "camera_task.h"
#include "camera_driver.h"
#include "shared_mem.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "freertos/task.h"
#include <string.h>

static const char* TAG = "camera_task";

static void vCameraTask(void* param)
{
    camera_msg_t msg;
    bool streaming = false;
    ESP_LOGI(TAG, "Camera task started on CORE %d", xPortGetCoreID());

    while (true) {
        if (!streaming) {
            // Normal mode: wait for one command at a time
            if (xQueueReceive(s_camera_queue, &msg, portMAX_DELAY) != pdTRUE) {
                continue;
            }
        } else {
            // Streaming mode: capture continuously without waiting for queue
            // (queue still receives STREAM_STOP to exit streaming)
            if (s_shared_state.camera_running) {
                camera_fb_t* fb = esp_camera_fb_get();
                if (fb && fb->len <= JPEG_BUF_SIZE) {
                    // Take mutex before writing to shared buffer
                    if (xSemaphoreTake(s_jpeg_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                        memcpy(s_jpeg_buf, fb->buf, fb->len);
                        msg.frame_len = fb->len;
                        xSemaphoreGive(s_jpeg_mutex);
                        // Notify CORE0 that frame is ready
                        xQueueSend(s_result_queue, &msg, 0);
                    } else {
                        ESP_LOGW(TAG, "JPEG mutex timeout in stream capture");
                        msg.frame_len = 0;
                        xQueueSend(s_result_queue, &msg, 0);
                    }
                } else if (fb) {
                    ESP_LOGW(TAG, "Frame too large or null: %d", fb->len ? fb->len : 0);
                    msg.frame_len = 0;
                    xQueueSend(s_result_queue, &msg, 0);
                }
                if (fb) esp_camera_fb_return(fb);

                // Check if streaming was stopped via queue (non-blocking check)
                camera_msg_t stop_msg;
                if (xQueueReceive(s_camera_queue, &stop_msg, pdMS_TO_TICKS(100)) == pdTRUE) {
                    msg = stop_msg;
                    streaming = false;
                    s_shared_state.stream_mode = false;
                    ESP_LOGI(TAG, "Stream mode stopped");
                    xQueueSend(s_result_queue, &msg, 0);
                    continue;
                }
                continue;
            } else {
                // Camera not running, exit stream mode
                streaming = false;
                s_shared_state.stream_mode = false;
                msg.cmd = CAMERA_CMD_STREAM_STOP;
                msg.frame_len = 0;
                xQueueSend(s_result_queue, &msg, 0);
                continue;
            }
        }

        // Non-streaming command processing
        ESP_LOGD(TAG, "Camera task got cmd: %d seq=%u", msg.cmd, msg.seq);

        switch (msg.cmd) {
            case CAMERA_CMD_INIT: {
                camera_err_t err = camera_driver_init();
                s_shared_state.camera_running = (err == CAMERA_OK);
                msg.frame_len = s_shared_state.camera_running ? 1 : 0;
                ESP_LOGI(TAG, "Camera init: %s", s_shared_state.camera_running ? "OK" : "FAIL");
                xQueueSend(s_result_queue, &msg, 0);
                break;
            }

            case CAMERA_CMD_DEINIT: {
                camera_driver_deinit();
                s_shared_state.camera_running = false;
                streaming = false;
                s_shared_state.stream_mode = false;
                msg.frame_len = 0;
                ESP_LOGI(TAG, "Camera deinit done");
                xQueueSend(s_result_queue, &msg, 0);
                break;
            }

            case CAMERA_CMD_CAPTURE: {
                if (!s_shared_state.camera_running) {
                    msg.frame_len = 0;
                    xQueueSend(s_result_queue, &msg, 0);
                    break;
                }
                camera_fb_t* fb = esp_camera_fb_get();
                if (!fb) {
                    ESP_LOGW(TAG, "Frame capture failed");
                    msg.frame_len = 0;
                    xQueueSend(s_result_queue, &msg, 0);
                    break;
                }
                if (fb->len > JPEG_BUF_SIZE) {
                    ESP_LOGE(TAG, "Frame too large: %d > %d", fb->len, JPEG_BUF_SIZE);
                    esp_camera_fb_return(fb);
                    msg.frame_len = 0;
                    xQueueSend(s_result_queue, &msg, 0);
                    break;
                }
                // Take mutex before writing shared buffer
                if (xSemaphoreTake(s_jpeg_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    memcpy(s_jpeg_buf, fb->buf, fb->len);
                    msg.frame_len = fb->len;
                    xSemaphoreGive(s_jpeg_mutex);
                } else {
                    ESP_LOGE(TAG, "JPEG mutex timeout");
                    msg.frame_len = 0;
                }
                esp_camera_fb_return(fb);
                ESP_LOGD(TAG, "Frame captured: %d bytes", msg.frame_len);
                xQueueSend(s_result_queue, &msg, 0);
                break;
            }

            case CAMERA_CMD_STREAM_START:
                if (s_shared_state.camera_running) {
                    streaming = true;
                    s_shared_state.stream_mode = true;
                    ESP_LOGI(TAG, "Stream mode started");
                } else {
                    ESP_LOGW(TAG, "Stream start ignored: camera not running");
                }
                msg.frame_len = 0;
                xQueueSend(s_result_queue, &msg, 0);
                break;

            case CAMERA_CMD_STREAM_STOP:
                streaming = false;
                s_shared_state.stream_mode = false;
                ESP_LOGI(TAG, "Stream mode stopped");
                msg.frame_len = 0;
                xQueueSend(s_result_queue, &msg, 0);
                break;

            default:
                break;
        }
    }
}

void camera_task_start(void)
{
    xTaskCreatePinnedToCore(&vCameraTask, "camera_task",
                            CAMERA_TASK_STACK_SIZE, NULL,
                            CAMERA_TASK_PRIORITY,
                            &s_camera_task_handle,
                            CAMERA_TASK_CORE);
    ESP_LOGI(TAG, "Camera task created on CORE %d", CAMERA_TASK_CORE);
}

void camera_task_stop(void)
{
    if (s_camera_task_handle) {
        vTaskDelete(s_camera_task_handle);
        s_camera_task_handle = NULL;
    }
}
