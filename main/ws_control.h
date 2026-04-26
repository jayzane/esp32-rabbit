#ifndef WS_CONTROL_H
#define WS_CONTROL_H

#include <stdbool.h>

typedef enum {
    WS_ACTION_ON,
    WS_ACTION_OFF,
    WS_ACTION_STATUS,
} ws_action_t;

typedef void (*ws_control_callback_t)(ws_action_t action, void* user_data);

void ws_control_init(ws_control_callback_t callback, void* user_data);
void ws_control_deinit(void);
void ws_control_send_response(const char* json_response);

#endif // WS_CONTROL_H
