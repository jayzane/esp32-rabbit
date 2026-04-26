#ifndef CAMERA_DRIVER_H
#define CAMERA_DRIVER_H

#include "esp_camera.h"

typedef enum {
    CAMERA_OK = 0,
    CAMERA_ERROR_INIT_FAILED = 1,
    CAMERA_ERROR_NOT_INITIALIZED = 2,
    CAMERA_ERROR_FRAME_TIMEOUT = 3,
} camera_err_t;

camera_err_t camera_driver_init(void);
void camera_driver_deinit(void);
camera_err_t camera_driver_start(void);
void camera_driver_stop(void);
bool camera_driver_is_running(void);
camera_fb_t* camera_driver_get_frame(void);
void camera_driver_return_frame(camera_fb_t* fb);
bool camera_driver_is_initialized(void);

#endif // CAMERA_DRIVER_H