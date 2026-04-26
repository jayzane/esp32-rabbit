#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>

#define SERVO_MIN_ANGLE 0
#define SERVO_MAX_ANGLE 180

void servo_init(void);
void servo_set_angle(int angle);
int servo_get_angle(void);
void servo_deinit(void);

#endif // SERVO_H
