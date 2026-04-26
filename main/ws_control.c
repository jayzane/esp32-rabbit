#include "ws_control.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"
#include <libwebsockets.h>

static const char *TAG = "ws_control";

#define WS_CONTROL_PORT 8080
#define WS_CONTROL_PATH "/control"

static ws_control_callback_t g_callback = NULL;
static void *g_user_data = NULL;
static struct lws *g_active_wsi = NULL;

static int ws_control_callback(struct lws *wsi, enum lws_callback_reasons reason,
                                void *user, void *in, size_t len)
{
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            char path_buf[128];
            lws_get_request_path(wsi, path_buf, sizeof(path_buf));
            ESP_LOGI(TAG, "WebSocket connection established from path: %s", path_buf);
            if (strcmp(path_buf, WS_CONTROL_PATH) != 0) {
                ESP_LOGW(TAG, "Rejected WebSocket connection from invalid path: %s", path_buf);
                g_active_wsi = NULL;
                return -1;  // reject connection
            }
            g_active_wsi = wsi;
            break;
        }

        case LWS_CALLBACK_RECEIVE: {
            char *message = (char *)in;
            size_t msg_len = len;

            // Ensure null-terminated string
            char *json_str = malloc(msg_len + 1);
            if (!json_str) {
                ESP_LOGE(TAG, "Failed to allocate memory for message");
                break;
            }
            memcpy(json_str, message, msg_len);
            json_str[msg_len] = '\0';

            ESP_LOGI(TAG, "Received: %s", json_str);

            // Parse JSON looking for "action" field
            ws_action_t action = WS_ACTION_STATUS;
            char *action_start = strstr(json_str, "\"action\"");
            if (action_start) {
                action_start = strchr(action_start, ':');
                if (action_start) {
                    action_start++;
                    // Skip whitespace and quotes
                    while (*action_start == ' ' || *action_start == '\"') {
                        action_start++;
                    }
                    if (strncmp(action_start, "on", 2) == 0) {
                        action = WS_ACTION_ON;
                    } else if (strncmp(action_start, "off", 3) == 0) {
                        action = WS_ACTION_OFF;
                    } else {
                        action = WS_ACTION_STATUS;
                    }
                }
            }

            ESP_LOGI(TAG, "Parsed action: %d", action);

            // Invoke stored callback with parsed action
            if (g_callback) {
                g_callback(action, g_user_data);
            }

            free(json_str);
            break;
        }

        case LWS_CALLBACK_CLOSED:
            ESP_LOGI(TAG, "WebSocket connection closed");
            g_active_wsi = NULL;
            break;

        case LWS_CALLBACK_HTTP:
            // Validate request path - reject non-control paths
            if (strcmp(in, WS_CONTROL_PATH) != 0) {
                ESP_LOGW(TAG, "Rejected HTTP request for path: %s", (char *)in);
                return 1;  // non-zero = reject
            }
            break;

        default:
            break;
    }

    return 0;
}

static struct lws_protocols ws_control_protocols[] = {
    {
        "ws_control",
        ws_control_callback,
        0,
        4096,
    },
    { NULL, NULL, 0, 0 }
};

static struct lws_context *g_context = NULL;

void ws_control_init(ws_control_callback_t callback, void *user_data)
{
    g_callback = callback;
    g_user_data = user_data;

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = WS_CONTROL_PORT;
    info.protocols = ws_control_protocols;
    info.options = LWS_SERVER_OPTION_HTTP;

    g_context = lws_create_context(&info);
    if (g_context == NULL) {
        ESP_LOGE(TAG, "Failed to create WebSocket context");
        return;
    }

    ESP_LOGI(TAG, "WebSocket server started on port %d%s", WS_CONTROL_PORT, WS_CONTROL_PATH);
}

void ws_control_deinit(void)
{
    if (g_context) {
        lws_cancel_service(g_context);
        lws_context_destroy(g_context);
        g_context = NULL;
    }
    g_callback = NULL;
    g_user_data = NULL;
    g_active_wsi = NULL;
    ESP_LOGI(TAG, "WebSocket server deinitialized");
}

void ws_control_send_response(const char *json_response)
{
    if (g_active_wsi == NULL || json_response == NULL) {
        ESP_LOGW(TAG, "Cannot send response: no active connection or null response");
        return;
    }

    unsigned char buf[LWS_PRE + 4096];
    unsigned char *p = &buf[LWS_PRE];

    size_t len = strlen(json_response);
    if (len > sizeof(buf) - LWS_PRE - 1) {
        ESP_LOGE(TAG, "Response too large");
        return;
    }

    memcpy(p, json_response, len);

    int n = lws_write(g_active_wsi, p, len, LWS_WRITE_TEXT);
    if (n < 0) {
        ESP_LOGE(TAG, "Failed to send WebSocket response");
    } else {
        ESP_LOGI(TAG, "Sent response: %s", json_response);
    }
}
