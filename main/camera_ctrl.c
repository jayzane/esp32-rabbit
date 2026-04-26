#include "camera_ctrl.h"
#include "esp_log.h"

static const char* TAG = "camera_ctrl";

static bool s_camera_on = false;

void camera_ctrl_init(void)
{
    ESP_LOGI(TAG, "Camera control initialized");
}

void camera_ctrl_deinit(void)
{
    camera_ctrl_disable();
    ESP_LOGI(TAG, "Camera control deinitialized");
}

camera_err_t camera_ctrl_enable(camera_err_t (*callback)(void*), void* user_data)
{
    if (!camera_driver_is_initialized()) {
        camera_err_t err = camera_driver_init();
        if (err != CAMERA_OK) {
            ESP_LOGE(TAG, "Failed to initialize camera driver");
            return err;
        }
    }

    camera_err_t err = camera_driver_start();
    if (err == CAMERA_OK) {
        s_camera_on = true;
        ESP_LOGI(TAG, "Camera enabled");
    }

    if (callback) {
        callback(user_data);
    }

    return err;
}

void camera_ctrl_disable(void)
{
    if (s_camera_on) {
        camera_driver_stop();
        s_camera_on = false;
        ESP_LOGI(TAG, "Camera disabled");
    }
}

bool camera_ctrl_is_on(void)
{
    return s_camera_on;
}