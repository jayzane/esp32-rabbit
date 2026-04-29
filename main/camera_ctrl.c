#include "camera_ctrl.h"
#include "shared_mem.h"
#include "esp_log.h"

static const char* TAG = "camera_ctrl";

// CORE0-side camera state (mirrors CORE1's s_shared_state.camera_running)
static bool s_camera_on = false;

void camera_ctrl_init(void)
{
    ESP_LOGI(TAG, "Camera ctrl init");
}

void camera_ctrl_deinit(void)
{
    s_camera_on = false;
}

camera_err_t camera_ctrl_enable(void)
{
    camera_msg_t msg = {
        .cmd = CAMERA_CMD_INIT,
        .seq = 0,
        .frame_len = 0,
    };
    if (xQueueSend(s_camera_queue, &msg, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send INIT cmd to camera task");
        return CAMERA_ERROR_INIT_FAILED;
    }

    // Wait for CORE1 response
    camera_msg_t result;
    if (xQueueReceive(s_result_queue, &result, pdMS_TO_TICKS(5000)) == pdTRUE) {
        if (result.frame_len > 0) {
            s_camera_on = true;
            return CAMERA_OK;
        }
    }
    return CAMERA_ERROR_INIT_FAILED;
}

void camera_ctrl_disable(void)
{
    camera_msg_t msg = {
        .cmd = CAMERA_CMD_DEINIT,
        .seq = 0,
        .frame_len = 0,
    };
    xQueueSend(s_camera_queue, &msg, pdMS_TO_TICKS(1000));
    s_camera_on = false;
    ESP_LOGI(TAG, "Camera disabled");
}

bool camera_ctrl_is_on(void)
{
    return s_camera_on;
}

size_t camera_ctrl_capture(void)
{
    camera_msg_t msg = {
        .cmd = CAMERA_CMD_CAPTURE,
        .seq = 0,
        .frame_len = 0,
    };
    if (xQueueSend(s_camera_queue, &msg, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return 0;
    }
    camera_msg_t result;
    if (xQueueReceive(s_result_queue, &result, pdMS_TO_TICKS(3000)) == pdTRUE) {
        return result.frame_len;
    }
    return 0;
}
