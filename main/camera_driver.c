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
    s_mutex = xSemaphoreCreateMutex();

    camera_config_t config = {
        .pin_pwdn = -1,
        .pin_reset = -1,
        .pin_xclk = 0,
        .pin_sscb_sda = 26,
        .pin_sscb_scl = 27,
        .pin_d7 = 35,
        .pin_d6 = 34,
        .pin_d5 = 39,
        .pin_d4 = 36,
        .pin_d3 = 21,
        .pin_d2 = 19,
        .pin_d1 = 18,
        .pin_d0 = 5,
        .pin_vsync = 25,
        .pin_href = 23,
        .pin_pclk = 22,
        .xclk_freq_hz = 10000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_VGA,
        .jpeg_quality = 8,
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return CAMERA_ERROR_INIT_FAILED;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_brightness(s, 0);
        s->set_contrast(s, 0);
        s->set_saturation(s, 0);
        s->set_whitebal(s, 1);
        s->set_awb_gain(s, 1);
        s->set_wb_mode(s, 0);
        s->set_exposure_ctrl(s, 1);
        s->set_ae_level(s, 0);
        s->set_aec_value(s, 300);
        s->set_gain_ctrl(s, 1);
        s->set_agc_gain(s, 30);
        s->set_gainceiling(s, 0);
        s->set_bpc(s, 0);
        s->set_wpc(s, 1);
        s->set_raw_gma(s, 1);
        s->set_lenc(s, 1);
        s->set_hmirror(s, 0);
        s->set_vflip(s, 0);
        s->set_dcw(s, 1);
        s->set_colorbar(s, 0);
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_initialized = true;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Camera initialized");
    return CAMERA_OK;
}

void camera_driver_deinit(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_initialized) {
        esp_camera_deinit();
        s_initialized = false;
        s_running = false;
        ESP_LOGI(TAG, "Camera deinitialized");
    }
    xSemaphoreGive(s_mutex);
    vSemaphoreDelete(s_mutex);
    s_mutex = NULL;
}

camera_err_t camera_driver_start(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (!s_initialized) {
        xSemaphoreGive(s_mutex);
        return CAMERA_ERROR_NOT_INITIALIZED;
    }
    s_running = true;
    xSemaphoreGive(s_mutex);
    return CAMERA_OK;
}

void camera_driver_stop(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_running = false;
    xSemaphoreGive(s_mutex);
}

bool camera_driver_is_running(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool running = s_running;
    xSemaphoreGive(s_mutex);
    return running;
}

camera_fb_t* camera_driver_get_frame(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (!s_running) {
        xSemaphoreGive(s_mutex);
        return NULL;
    }
    xSemaphoreGive(s_mutex);
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
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool initialized = s_initialized;
    xSemaphoreGive(s_mutex);
    return initialized;
}