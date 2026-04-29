#ifndef CAMERA_TASK_H
#define CAMERA_TASK_H

#include "shared_mem.h"

// CORE1 task stack size and priority
#define CAMERA_TASK_STACK_SIZE 4096
#define CAMERA_TASK_PRIORITY 5
#define CAMERA_TASK_CORE 1

void camera_task_start(void);
void camera_task_stop(void);

#endif // CAMERA_TASK_H
