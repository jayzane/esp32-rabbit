#include "shared_mem.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static const char* TAG = "shared_mem";

uint8_t s_jpeg_buf[JPEG_BUF_SIZE] IRAM_ATTR __attribute__((section(".psram_vars")));

QueueHandle_t s_camera_queue = NULL;
QueueHandle_t s_result_queue = NULL;
TaskHandle_t s_camera_task_handle = NULL;

shared_state_t s_shared_state = {
    .camera_running = false,
    .stream_mode = false,
    .last_seq = 0,
};

void shared_mem_init(void)
{
    // Verify PSRAM availability
    if (heap_caps_check_integrity_all(MALLOC_CAP_SPIRAM)) {
        ESP_LOGI(TAG, "PSRAM available, JPEG buf in PSRAM");
    } else {
        ESP_LOGW(TAG, "PSRAM not available, using DRAM (JPEG buf may be small)");
    }

    s_camera_queue = xQueueCreate(4, sizeof(camera_msg_t));
    s_result_queue = xQueueCreate(4, sizeof(camera_msg_t));

    if (!s_camera_queue || !s_result_queue) {
        ESP_LOGE(TAG, "Failed to create queues");
        abort();
    }

    ESP_LOGI(TAG, "Shared mem init done. PSRAM buf: %d bytes", JPEG_BUF_SIZE);
}

void shared_mem_deinit(void)
{
    if (s_camera_queue) {
        vQueueDelete(s_camera_queue);
        s_camera_queue = NULL;
    }
    if (s_result_queue) {
        vQueueDelete(s_result_queue);
        s_result_queue = NULL;
    }
}
