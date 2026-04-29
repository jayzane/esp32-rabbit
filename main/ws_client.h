#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Command types received from server
typedef enum {
    WS_CMD_UNKNOWN,
    WS_CMD_CAMERA_ON,
    WS_CMD_CAMERA_OFF,
    WS_CMD_SERVO,
    WS_CMD_CAPTURE,
    WS_CMD_STREAM_START,
    WS_CMD_STREAM_STOP,
} ws_cmd_type_t;

// Parsed command from server
typedef struct {
    ws_cmd_type_t type;
    uint32_t seq;
    int angle;  // for SERVO command
} ws_cmd_t;

// Callback for received commands
typedef void (*ws_client_event_callback_t)(ws_cmd_t* cmd, void* user_data);

void ws_client_init(ws_client_event_callback_t callback, void* user_data);
void ws_client_deinit(void);

// Send text message (JSON response)
void ws_client_send_text(const char* json_response);
// Send binary data (e.g., JPEG frame)
void ws_client_send_binary(const uint8_t* data, size_t len);

// Connection status
bool ws_client_is_connected(void);

// WebSocket server URI
#ifndef WS_SERVER_URI
#define WS_SERVER_URI "ws://10.0.0.232:11080"
#endif

#define WS_PING_INTERVAL_MS       10000
#define WS_RECONNECT_MAX_DELAY_MS 30000

#endif // WS_CLIENT_H