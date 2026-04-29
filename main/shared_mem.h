#ifndef SHARED_MEM_H
#define SHARED_MEM_H

#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define JPEG_BUF_SIZE (128 * 1024)  // 128KB PSRAM

// CORE0 → CORE1 commands
typedef enum {
    CAMERA_CMD_NONE,
    CAMERA_CMD_INIT,
    CAMERA_CMD_DEINIT,
    CAMERA_CMD_CAPTURE,
    CAMERA_CMD_STREAM_START,
    CAMERA_CMD_STREAM_STOP,
} camera_cmd_t;

// Queue message structure
typedef struct {
    camera_cmd_t cmd;
    uint32_t seq;          // sequence number, CORE0 tracks responses
    size_t frame_len;      // filled by CORE1: JPEG data length
} camera_msg_t;

// PSRAM shared JPEG buffer (runtime allocated)
extern uint8_t* s_jpeg_buf;

// Binary semaphore protecting s_jpeg_buf
// CORE1 takes before writing, CORE0 takes before reading
extern SemaphoreHandle_t s_jpeg_mutex;

// Dual-core communication queues
extern QueueHandle_t s_camera_queue;   // CORE0 → CORE1
extern QueueHandle_t s_result_queue;   // CORE1 → CORE0

// CORE1 task handle (for notifications)
extern TaskHandle_t s_camera_task_handle;

// Shared state (DRAM)
typedef struct {
    volatile bool camera_running;
    volatile bool stream_mode;
    volatile uint32_t last_seq;
} shared_state_t;

extern shared_state_t s_shared_state;

// Initialize shared memory and queues
void shared_mem_init(void);
void shared_mem_deinit(void);

#endif // SHARED_MEM_H
