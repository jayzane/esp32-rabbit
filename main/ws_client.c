#include "ws_client.h"
#include "shared_mem.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#include "esp_event.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "ws_client";

static esp_websocket_client_handle_t s_ws_client = NULL;
static ws_client_event_callback_t s_event_callback = NULL;
static void* s_user_data = NULL;
static bool s_connected = false;
static TaskHandle_t s_reconnect_task = NULL;
static uint32_t s_seq_counter = 0;

static void ws_client_reconnect_task(void* param);
static void parse_json_command(const char* json, ws_cmd_t* out_cmd);

static void log_error_if_nonzero(const char* message, int err_code)
{
    if (err_code != 0) {
        ESP_LOGE(TAG, "%s: %d", message, err_code);
    }
}

static uint32_t next_seq(void)
{
    return ++s_seq_counter;
}

static esp_err_t websocket_event_handler(void* handler_args,
                                        esp_event_base_t base,
                                        int32_t event_id,
                                        void* event_data)
{
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WS connected to " WS_SERVER_URI);
        s_connected = true;
        if (s_reconnect_task) {
            vTaskDelete(s_reconnect_task);
            s_reconnect_task = NULL;
        }
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WS disconnected");
        s_connected = false;
        log_error_if_nonzero("WS disconnect reason:", data->error_handle->error_type);
        xTaskCreate(&ws_client_reconnect_task, "ws_reconnect", 4096, NULL, 3, &s_reconnect_task);
        break;

    case WEBSOCKET_EVENT_DATA:
        ESP_LOGD(TAG, "WS data received: opcode=%d len=%d",
                 data->op_code, data->data_len);

        if (data->op_code == WS_TRANSPORT_OPCODE_TEXT) {
            ws_cmd_t cmd = {0};
            parse_json_command(data->data_ptr, &cmd);
            if (s_event_callback) {
                s_event_callback(&cmd, s_user_data);
            }
        } else if (data->op_code == WS_TRANSPORT_OPCODE_PING) {
            ESP_LOGD(TAG, "WS ping received");
        } else if (data->op_code == WS_TRANSPORT_OPCODE_CLOSE) {
            ESP_LOGI(TAG, "WS close received");
        }
        break;

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WS error");
        log_error_if_nonzero("WS error:", data->error_handle->error_type);
        break;

    default:
        break;
    }
    return ESP_OK;
}

static void parse_json_command(const char* json, ws_cmd_t* out_cmd)
{
    memset(out_cmd, 0, sizeof(*out_cmd));
    out_cmd->type = WS_CMD_UNKNOWN;
    out_cmd->seq = next_seq();

    const char* cmd_start = strstr(json, "\"cmd\"");
    if (!cmd_start) return;

    const char* colon = strchr(cmd_start, ':');
    if (!colon) return;
    colon++;
    while (*colon == ' ' || *colon == '"') colon++;

    if (strncmp(colon, "camera_on", 9) == 0) {
        out_cmd->type = WS_CMD_CAMERA_ON;
    } else if (strncmp(colon, "camera_off", 10) == 0) {
        out_cmd->type = WS_CMD_CAMERA_OFF;
    } else if (strncmp(colon, "servo", 5) == 0) {
        out_cmd->type = WS_CMD_SERVO;
        const char* angle_start = strstr(json, "\"angle\"");
        if (angle_start) {
            const char* a_colon = strchr(angle_start, ':');
            if (a_colon) {
                out_cmd->angle = atoi(a_colon + 1);
            }
        }
    } else if (strncmp(colon, "capture", 7) == 0) {
        out_cmd->type = WS_CMD_CAPTURE;
    } else if (strncmp(colon, "stream_start", 12) == 0) {
        out_cmd->type = WS_CMD_STREAM_START;
    } else if (strncmp(colon, "stream_stop", 11) == 0) {
        out_cmd->type = WS_CMD_STREAM_STOP;
    }

    ESP_LOGI(TAG, "Parsed cmd: type=%d seq=%u angle=%d", out_cmd->type, out_cmd->seq, out_cmd->angle);
}

static void ws_client_reconnect_task(void* param)
{
    int delay_ms = 1000;
    while (1) {
        ESP_LOGI(TAG, "WS reconnecting in %d ms...", delay_ms);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));

        if (s_ws_client) {
            esp_websocket_client_start(s_ws_client);
        }

        delay_ms *= 2;
        if (delay_ms > WS_RECONNECT_MAX_DELAY_MS) {
            delay_ms = WS_RECONNECT_MAX_DELAY_MS;
        }

        if (s_connected) break;
    }
    vTaskDelete(NULL);
}

void ws_client_init(ws_client_event_callback_t callback, void* user_data)
{
    s_event_callback = callback;
    s_user_data = user_data;

    esp_websocket_client_config_t cfg = {
        .uri = WS_SERVER_URI,
        .ping_interval_ms = WS_PING_INTERVAL_MS,
        .pingpong_timeout_secs = 10,
    };

    s_ws_client = esp_websocket_client_init(&cfg);
    if (!s_ws_client) {
        ESP_LOGE(TAG, "Failed to create WS client");
        return;
    }

    esp_websocket_client_register_event(s_ws_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);

    esp_err_t err = esp_websocket_client_start(s_ws_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WS start failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "WS client starting...");
    }
}

void ws_client_deinit(void)
{
    if (s_ws_client) {
        esp_websocket_client_stop(s_ws_client);
        esp_websocket_client_destroy(s_ws_client);
        s_ws_client = NULL;
    }
    s_connected = false;
}

void ws_client_send_text(const char* json_response)
{
    if (!s_ws_client || !s_connected) {
        ESP_LOGW(TAG, "WS not connected, cannot send text");
        return;
    }
    int len = strlen(json_response);
    int wlen = esp_websocket_client_send(s_ws_client, json_response, len, portMAX_DELAY);
    if (wlen < len) {
        ESP_LOGW(TAG, "WS text sent partial: %d/%d", wlen, len);
    }
}

void ws_client_send_binary(const uint8_t* data, size_t len)
{
    if (!s_ws_client || !s_connected) {
        ESP_LOGW(TAG, "WS not connected, cannot send binary");
        return;
    }
    int wlen = esp_websocket_client_send(s_ws_client, (const char*)data, len, portMAX_DELAY);
    if (wlen < (int)len) {
        ESP_LOGW(TAG, "WS binary sent partial: %d/%d", wlen, (int)len);
    }
}

bool ws_client_is_connected(void)
{
    return s_connected;
}
