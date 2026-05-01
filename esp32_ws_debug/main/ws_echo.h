#ifndef WS_ECHO_H
#define WS_ECHO_H

#include <stdint.h>
#include <stdbool.h>

#define WS_ECHO_SERVER_URI  "ws://10.0.0.232:11081/debug"
#define WS_ECHO_HEARTBEAT_MS 5000
#define WS_ECHO_RECONNECT_MAX_MS 30000

typedef enum {
    WS_ECHO_DISCONNECTED = 0,
    WS_ECHO_CONNECTING,
    WS_ECHO_CONNECTED,
} ws_echo_state_t;

typedef void (*ws_echo_on_receive_t)(const char* data, int len);

void ws_echo_init(ws_echo_on_receive_t callback);
void ws_echo_deinit(void);
bool ws_echo_is_connected(void);
void ws_echo_send(const char* json);

#endif // WS_ECHO_H