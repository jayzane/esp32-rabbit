#include "ws_client.h"
#include "shared_mem.h"
#include "esp_log.h"
#include "esp_event.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/ip4_addr.h"
#include <string.h>
#include "esp_random.h"

static const char* TAG = "ws_client";

// WebSocket opcodes
#define WS_OPCODE_TEXT   0x01
#define WS_OPCODE_BINARY 0x02
#define WS_OPCODE_CLOSE  0x08

static ws_client_event_callback_t s_event_callback = NULL;
static void* s_user_data = NULL;
static bool s_connected = false;
static TaskHandle_t s_reconnect_task = NULL;
static TaskHandle_t s_recv_task = NULL;
static int s_sock_fd = -1;
static uint32_t s_seq_counter = 0;

// Base64 encoding table
static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void ws_client_reconnect_task(void* param);
static void ws_client_recv_task(void* param);
static void parse_json_command(const char* json, ws_cmd_t* out_cmd);
static bool ws_send_handshake(int sock, const char* host, const char* path);
static int ws_parse_frame(const uint8_t* data, int len, uint8_t* out_payload, int max_len);

static uint32_t next_seq(void)
{
    return ++s_seq_counter;
}

// Base64 encode (minimal implementation for WS handshake)
static int base64_encode(const uint8_t* in, int in_len, char* out)
{
    int i, j;
    for (i = 0, j = 0; i < in_len; i += 3) {
        int a = in[i];
        int b = (i + 1 < in_len) ? in[i + 1] : 0;
        int c = (i + 2 < in_len) ? in[i + 2] : 0;
        int triplet = (a << 16) | (b << 8) | c;
        out[j++] = base64_table[(triplet >> 18) & 0x3F];
        out[j++] = base64_table[(triplet >> 12) & 0x3F];
        out[j++] = (i + 1 < in_len) ? base64_table[(triplet >> 6) & 0x3F] : '=';
        out[j++] = (i + 2 < in_len) ? base64_table[triplet & 0x3F] : '=';
    }
    out[j] = '\0';
    return j;
}

// Compute SHA1 for WebSocket key (WS handshake)
static void sha1_hash(const char* data, int len, uint8_t* out)
{
    // Simple SHA1 for Sec-WebSocket-Key
    // RFC 6455: Sec-WebSocket-Key is base64 encoded 16-byte random
    // For simplicity, we compute a hash of the key + magic string
    memset(out, 0, 20);
    // Use a simple hash since we just need to prove we can compute it
    for (int i = 0; i < len && i < 64; i++) {
        out[i % 20] ^= (uint8_t)data[i];
        out[(i * 7) % 20] ^= (uint8_t)(data[i] >> 3);
    }
}

static bool ws_send_handshake(int sock, const char* host, const char* path)
{
    // Generate random 16-byte key
    uint8_t key_bin[16];
    for (int i = 0; i < 16; i++) {
        key_bin[i] = (uint8_t)(esp_random() & 0xFF);
    }
    char key_b64[32];
    base64_encode(key_bin, 16, key_b64);

    // WebSocket handshake request
    char request[512];
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        path, host, key_b64);

    int ret = send(sock, request, strlen(request), 0);
    if (ret < 0) {
        ESP_LOGE(TAG, "Handshake send failed");
        return false;
    }

    // Read response
    char response[512];
    int received = recv(sock, response, sizeof(response) - 1, 0);
    if (received <= 0) {
        ESP_LOGE(TAG, "Handshake response read failed: %d", received);
        return false;
    }
    response[received] = '\0';

    // Check for "HTTP/1.1 101" switching protocols
    if (strstr(response, "101") == NULL) {
        ESP_LOGE(TAG, "WS handshake failed, response: %s", response);
        return false;
    }

    ESP_LOGI(TAG, "WS handshake OK");
    return true;
}

static int ws_parse_frame(const uint8_t* data, int len, uint8_t* out_payload, int max_len)
{
    if (len < 2) return -1;

    int opcode = data[0] & 0x0F;
    int masked = (data[1] & 0x80) != 0;
    int payload_len = data[1] & 0x7F;

    int header_len = 2;
    if (payload_len == 126) {
        if (len < 4) return -1;
        payload_len = (data[2] << 8) | data[3];
        header_len = 4;
    } else if (payload_len == 127) {
        if (len < 10) return -1;
        // For simplicity, ignore > 64KB frames
        return -1;
    }

    int mask_len = masked ? 4 : 0;
    if (len < header_len + mask_len + payload_len) return -1;

    if (opcode == 0x08) {
        // Close frame
        return -2;
    }

    if (opcode != 0x01 && opcode != 0x02) {
        // Ignore other opcodes (ping/pong/continuation)
        return 0;
    }

    const uint8_t* payload = data + header_len + mask_len;
    int out_len = payload_len;
    if (out_len > max_len) out_len = max_len;

    if (masked) {
        const uint8_t* mask = data + header_len;
        for (int i = 0; i < out_len; i++) {
            out_payload[i] = payload[i] ^ mask[i % 4];
        }
    } else {
        memcpy(out_payload, payload, out_len);
    }

    return out_len;
}

static void ws_client_recv_task(void* param)
{
    ESP_LOGI(TAG, "WS recv task STARTED, s_sock_fd=%d", s_sock_fd);
    uint8_t buf[2048];
    uint8_t payload[2048];
    int loop_count = 0;

    while (s_sock_fd >= 0) {
        loop_count++;
        if (loop_count % 1000 == 0) {
            ESP_LOGI(TAG, "WS recv task alive, loop=%d", loop_count);
        }
        int received = recv(s_sock_fd, (char*)buf, sizeof(buf) - 1, 0);
        ESP_LOGI(TAG, "WS recv returned: %d bytes", received);
        if (received <= 0) {
            ESP_LOGI(TAG, "WS recv disconnected, errno=%d", errno);
            break;
        }

        // Log ALL bytes received for debugging
        ESP_LOGI(TAG, "WS recv raw bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                 buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
                 buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);

        int payload_len = ws_parse_frame(buf, received, payload, sizeof(payload) - 1);
        ESP_LOGI(TAG, "WS parse result: %d", payload_len);
        if (payload_len > 0) {
            payload[payload_len] = '\0';
            ESP_LOGI(TAG, "WS text received: %.*s", payload_len, payload);
            ws_cmd_t cmd = {0};
            parse_json_command((char*)payload, &cmd);
            ESP_LOGI(TAG, "WS cmd parsed: type=%d", cmd.type);
            if (s_event_callback) {
                ESP_LOGI(TAG, "WS invoking callback");
                s_event_callback(&cmd, s_user_data);
            }
        } else if (payload_len == -2) {
            ESP_LOGI(TAG, "WS close frame received");
            break;
        } else if (payload_len == -1) {
            ESP_LOGW(TAG, "WS parse failed - incomplete frame");
        } else if (payload_len == 0) {
            ESP_LOGW(TAG, "WS parse skipped - non-text opcode");
        }
    }

    s_connected = false;
    if (s_sock_fd >= 0) {
        close(s_sock_fd);
        s_sock_fd = -1;
    }

    // Restart reconnect task
    if (s_reconnect_task == NULL) {
        xTaskCreate(&ws_client_reconnect_task, "ws_reconnect", 4096, NULL, 3, &s_reconnect_task);
    }
    vTaskDelete(NULL);
}

static void ws_client_reconnect_task(void* param)
{
    int delay_ms = 1000;
    struct sockaddr_in server_addr;

    while (1) {
        ESP_LOGI(TAG, "WS connecting to " WS_SERVER_URI " in %d ms...", delay_ms);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));

        // Parse WS_SERVER_URI (format: ws://host:port/path)
        const char* host_start = strstr(WS_SERVER_URI, "://");
        if (!host_start) {
            ESP_LOGE(TAG, "Invalid WS URI");
            break;
        }
        host_start += 3;
        const char* port_str = strchr(host_start, ':');
        const char* path_start = strchr(host_start, '/');
        char host[64] = {0};
        int port = 8080;
        char path[64] = "/";

        if (port_str && (!path_start || port_str < path_start)) {
            int host_len = port_str - host_start;
            if (host_len > 63) host_len = 63;
            memcpy(host, host_start, host_len);
            host[host_len] = '\0';
            port = atoi(port_str + 1);
        } else {
            int host_len = path_start ? (path_start - host_start) : strlen(host_start);
            if (host_len > 63) host_len = 63;
            memcpy(host, host_start, host_len);
            host[host_len] = '\0';
        }
        if (path_start) {
            strncpy(path, path_start, sizeof(path) - 1);
        }

        // Create socket
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            ESP_LOGE(TAG, "Socket create failed");
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            continue;
        }

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        inet_aton(host, &server_addr.sin_addr);

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
            ESP_LOGW(TAG, "WS connect failed, closing socket");
            close(sock);
            sock = -1;
            delay_ms *= 2;
            if (delay_ms > WS_RECONNECT_MAX_DELAY_MS) {
                delay_ms = WS_RECONNECT_MAX_DELAY_MS;
            }
            continue;
        }

        // Send WS handshake
        if (!ws_send_handshake(sock, host, path)) {
            close(sock);
            sock = -1;
            delay_ms *= 2;
            if (delay_ms > WS_RECONNECT_MAX_DELAY_MS) {
                delay_ms = WS_RECONNECT_MAX_DELAY_MS;
            }
            continue;
        }

        // Connected!
        s_sock_fd = sock;
        s_connected = true;
        ESP_LOGI(TAG, "WS connected to %s:%d%s", host, port, path);

        if (s_reconnect_task) {
            vTaskDelete(s_reconnect_task);
            s_reconnect_task = NULL;
        }

        // Start receive task
        xTaskCreate(&ws_client_recv_task, "ws_recv", 4096, NULL, 3, &s_recv_task);
        vTaskDelete(NULL);
        break;
    }
    vTaskDelete(NULL);
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

void ws_client_init(ws_client_event_callback_t callback, void* user_data)
{
    s_event_callback = callback;
    s_user_data = user_data;
    s_connected = false;
    s_sock_fd = -1;

    xTaskCreate(&ws_client_reconnect_task, "ws_reconnect", 4096, NULL, 3, &s_reconnect_task);
}

void ws_client_deinit(void)
{
    s_event_callback = NULL;
    s_user_data = NULL;

    if (s_recv_task) {
        vTaskDelete(s_recv_task);
        s_recv_task = NULL;
    }
    if (s_reconnect_task) {
        vTaskDelete(s_reconnect_task);
        s_reconnect_task = NULL;
    }
    if (s_sock_fd >= 0) {
        close(s_sock_fd);
        s_sock_fd = -1;
    }
    s_connected = false;
}

static bool ws_send_frame(int sock, const uint8_t* data, int len, int opcode)
{
    if (sock < 0 || !s_connected) return false;

    // WS frame: FIN=1, opcode=binary/text
    uint8_t header[10];
    int header_len = 2;
    header[0] = 0x80 | opcode;  // FIN + opcode
    if (len < 126) {
        header[1] = (uint8_t)len;
    } else if (len < 65536) {
        header[1] = 126;
        header[2] = (uint8_t)(len >> 8);
        header[3] = (uint8_t)(len & 0xFF);
        header_len = 4;
    } else {
        return false;  // Too large
    }

    // Send header
    if (send(sock, (char*)header, header_len, 0) != header_len) {
        return false;
    }

    // Send payload
    int sent = send(sock, (char*)data, len, 0);
    return sent == len;
}

void ws_client_send_text(const char* json_response)
{
    if (!ws_send_frame(s_sock_fd, (const uint8_t*)json_response, strlen(json_response), WS_OPCODE_TEXT)) {
        ESP_LOGW(TAG, "WS text send failed");
    }
}

void ws_client_send_binary(const uint8_t* data, size_t len)
{
    if (!ws_send_frame(s_sock_fd, data, len, WS_OPCODE_BINARY)) {
        ESP_LOGW(TAG, "WS binary send failed");
    }
}

bool ws_client_is_connected(void)
{
    return s_connected;
}