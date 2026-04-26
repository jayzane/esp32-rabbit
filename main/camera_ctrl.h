#ifndef CAMERA_CTRL_H
#define CAMERA_CTRL_H

#include <stdbool.h>
#include "camera_driver.h"

void camera_ctrl_init(void);
void camera_ctrl_deinit(void);
camera_err_t camera_ctrl_enable(camera_err_t (*callback)(void*), void* user_data);
void camera_ctrl_disable(void);
bool camera_ctrl_is_on(void);

#endif // CAMERA_CTRL_H