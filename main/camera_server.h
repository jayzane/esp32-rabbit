#ifndef CAMERA_SERVER_H
#define CAMERA_SERVER_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t camera_server_start(void);
void camera_server_stop(void);
bool camera_server_is_running(void);

#endif // CAMERA_SERVER_H