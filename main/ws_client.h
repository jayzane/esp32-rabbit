#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_websocket_client.h"

#define WS_SERVER_URI "ws://10.0.0.232:8080"
#define WS_RECONNECT_MAX_DELAY_MS 30000
#define WS_PING_INTERVAL_MS 30000

typedef enum {
    WS_CMD_CAMERA_ON,
    WS_CMD_CAMERA_OFF,
    WS_CMD_SERVO,
    WS_CMD_CAPTURE,
    WS_CMD_STREAM_START,
    WS_CMD_STREAM_STOP,
    WS_CMD_UNKNOWN,
} ws_cmd_type_t;

// Parsed command structure
typedef struct {
    ws_cmd_type_t type;
    uint32_t seq;
    int angle;        // for WS_CMD_SERVO
} ws_cmd_t;

// WS client event callback
typedef void (*ws_client_event_callback_t)(ws_cmd_t* cmd, void* user_data);

void ws_client_init(ws_client_event_callback_t callback, void* user_data);
void ws_client_deinit(void);
void ws_client_send_text(const char* json_response);
void ws_client_send_binary(const uint8_t* data, size_t len);
bool ws_client_is_connected(void);

#endif // WS_CLIENT_H
