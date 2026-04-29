#ifndef CAMERA_CTRL_H
#define CAMERA_CTRL_H

#include <stdbool.h>
#include "shared_mem.h"
#include "camera_driver.h"

// Queue-driven camera control API
void camera_ctrl_init(void);
void camera_ctrl_deinit(void);
camera_err_t camera_ctrl_enable(void);
void camera_ctrl_disable(void);
bool camera_ctrl_is_on(void);

// Trigger one frame capture (sync wait)
// Returns frame_len, 0 means failure
size_t camera_ctrl_capture(void);

#endif // CAMERA_CTRL_H
