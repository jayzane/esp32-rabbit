#include "camera_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char* TAG = "camera_driver";

static DRAM_ATTR SemaphoreHandle_t s_mutex = NULL;
static bool s_initialized = false;
static bool s_running = false;

camera_err_t camera_driver_init(void)
{
    if (s_initialized) {
        return CAMERA_OK;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return CAMERA_ERROR_INIT_FAILED;
    }

    camera_config_t config = {
        .pin_pwdn = -1,
        .pin_reset = -1,
        .pin_xclk = 21,
        .pin_sccb_sda = 26,
        .pin_sccb_scl = 27,
        .pin_d7 = 35,
        .pin_d6 = 34,
        .pin_d5 = 39,
        .pin_d4 = 36,
        .pin_d3 = 19,
        .pin_d2 = 18,
        .pin_d1 = 5,
        .pin_d0 = 4,
        .pin_vsync = 25,
        .pin_href = 23,
        .pin_pclk = 22,
        .xclk_freq_hz = 10000000,
        .ledc_timer = LEDC_TIMER_1,
        .ledc_channel = LEDC_CHANNEL_2,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_QVGA,
        .jpeg_quality = 25,
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_DRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s (0x%x)", esp_err_to_name(err), err);
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return CAMERA_ERROR_INIT_FAILED;
    }
    ESP_LOGI(TAG, "esp_camera_init OK");

    sensor_t* s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_pixformat(s, PIXFORMAT_JPEG);
        s->set_framesize(s, FRAMESIZE_QVGA);
        s->set_quality(s, 25);
        s->set_brightness(s, 0);
        s->set_contrast(s, 0);
        s->set_saturation(s, 0);
        s->set_whitebal(s, 1);
        s->set_awb_gain(s, 1);
        s->set_wb_mode(s, 0);
        s->set_exposure_ctrl(s, 1);
        s->set_ae_level(s, 0);
        s->set_aec_value(s, 120);
        s->set_gain_ctrl(s, 1);
        s->set_agc_gain(s, 0);
        s->set_gainceiling(s, GAINCEILING_2X);
        s->set_bpc(s, 0);
        s->set_wpc(s, 1);
        s->set_raw_gma(s, 1);
        s->set_lenc(s, 1);
        s->set_hmirror(s, 0);
        s->set_vflip(s, 1);
        s->set_dcw(s, 0);
        s->set_colorbar(s, 0);
        ESP_LOGI(TAG, "Sensor configured");
    } else {
        ESP_LOGE(TAG, "Failed to get camera sensor");
        esp_camera_deinit();
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return CAMERA_ERROR_INIT_FAILED;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Camera initialized: OV2640, JPEG, QVGA");
    return CAMERA_OK;
}

void camera_driver_deinit(void)
{
    if (s_initialized) {
        esp_camera_deinit();
        s_initialized = false;
        s_running = false;
        ESP_LOGI(TAG, "Camera deinitialized");
    }
    if (s_mutex) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }
}

camera_err_t camera_driver_start(void)
{
    if (!s_initialized) {
        return CAMERA_ERROR_NOT_INITIALIZED;
    }
    s_running = true;
    return CAMERA_OK;
}

void camera_driver_stop(void)
{
    s_running = false;
}

bool camera_driver_is_running(void)
{
    return s_running;
}

camera_fb_t* camera_driver_get_frame(void)
{
    if (!s_running) {
        return NULL;
    }
    return esp_camera_fb_get();
}

void camera_driver_return_frame(camera_fb_t* fb)
{
    if (fb != NULL) {
        esp_camera_fb_return(fb);
    }
}

bool camera_driver_is_initialized(void)
{
    return s_initialized;
}