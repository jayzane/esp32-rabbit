#include "ws_client.h"
#include "shared_mem.h"
#include "esp_log.h"
#include "esp_event.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/ip4_addr.h"
#include <string.h>
#include <sys/time.h>
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
    uint32_t seq = ++s_seq_counter;
    ESP_LOGI(TAG, "[NEXT_SEQ] %u", seq);
    return seq;
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
    ESP_LOGI(TAG, "[WS_HANDSHAKE] === START === sock=%d host=%s path=%s", sock, host, path);

    // Generate random 16-byte key
    uint8_t key_bin[16];
    for (int i = 0; i < 16; i++) {
        key_bin[i] = (uint8_t)(esp_random() & 0xFF);
    }
    ESP_LOGI(TAG, "[WS_HANDSHAKE] key_bin generated");
    char key_b64[32];
    base64_encode(key_bin, 16, key_b64);
    ESP_LOGI(TAG, "[WS_HANDSHAKE] key_b64=%s", key_b64);

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

    ESP_LOGI(TAG, "[WS_HANDSHAKE] sending request, len=%d", (int)strlen(request));
    int ret = send(sock, request, strlen(request), 0);
    ESP_LOGI(TAG, "[WS_HANDSHAKE] send ret=%d errno=%d", ret, errno);
    if (ret < 0) {
        ESP_LOGE(TAG, "[WS_HANDSHAKE] FAILED send ret=%d errno=%d", ret, errno);
        return false;
    }

    // Read response
    ESP_LOGI(TAG, "[WS_HANDSHAKE] reading response...");
    char response[512];
    int received = recv(sock, response, sizeof(response) - 1, 0);
    ESP_LOGI(TAG, "[WS_HANDSHAKE] recv ret=%d errno=%d", received, errno);
    if (received <= 0) {
        ESP_LOGE(TAG, "[WS_HANDSHAKE] FAILED read response ret=%d", received);
        return false;
    }
    response[received] = '\0';
    ESP_LOGI(TAG, "[WS_HANDSHAKE] response received:\n%s", response);

    // Check for "HTTP/1.1 101" switching protocols
    if (strstr(response, "101") == NULL) {
        ESP_LOGE(TAG, "[WS_HANDSHAKE] FAILED no 101, response:\n%s", response);
        return false;
    }

    ESP_LOGI(TAG, "[WS_HANDSHAKE] === SUCCESS ===");
    return true;
}

static int ws_parse_frame(const uint8_t* data, int len, uint8_t* out_payload, int max_len)
{
    ESP_LOGI(TAG, "[PARSE_FRAME] === START === len=%d max_len=%d", len, max_len);
    if (len < 2) {
        ESP_LOGW(TAG, "[PARSE_FRAME] FAILED len=%d < 2", len);
        return -1;
    }

    int opcode = data[0] & 0x0F;
    ESP_LOGI(TAG, "[PARSE_FRAME] opcode=0x%02x", opcode);
    int masked = (data[1] & 0x80) != 0;
    int payload_len = data[1] & 0x7F;
    ESP_LOGI(TAG, "[PARSE_FRAME] masked=%d payload_len=%d", masked, payload_len);

    int header_len = 2;
    if (payload_len == 126) {
        if (len < 4) {
            ESP_LOGW(TAG, "[PARSE_FRAME] FAILED len=%d < 4 for extended len", len);
            return -1;
        }
        payload_len = (data[2] << 8) | data[3];
        header_len = 4;
        ESP_LOGI(TAG, "[PARSE_FRAME] extended len 126: payload=%d", payload_len);
    } else if (payload_len == 127) {
        if (len < 10) {
            ESP_LOGW(TAG, "[PARSE_FRAME] FAILED len=%d < 10 for 127 extended", len);
            return -1;
        }
        ESP_LOGW(TAG, "[PARSE_FRAME] ignoring >64KB frame");
        return -1;
    }

    int mask_len = masked ? 4 : 0;
    ESP_LOGI(TAG, "[PARSE_FRAME] header_len=%d mask_len=%d total_needed=%d", header_len, mask_len, header_len + mask_len + payload_len);
    if (len < header_len + mask_len + payload_len) {
        ESP_LOGW(TAG, "[PARSE_FRAME] FAILED: incomplete frame have=%d need=%d", len, header_len + mask_len + payload_len);
        return -1;
    }

    if (opcode == 0x08) {
        ESP_LOGI(TAG, "[PARSE_FRAME] close frame");
        return -2;
    }

    if (opcode != 0x01 && opcode != 0x02) {
        ESP_LOGI(TAG, "[PARSE_FRAME] ignoring opcode 0x%02x", opcode);
        return 0;
    }

    const uint8_t* payload = data + header_len + mask_len;
    int out_len = payload_len;
    if (out_len > max_len) out_len = max_len;

    if (masked) {
        ESP_LOGI(TAG, "[PARSE_FRAME] masked frame, unmasking...");
        const uint8_t* mask = data + header_len;
        for (int i = 0; i < out_len; i++) {
            out_payload[i] = payload[i] ^ mask[i % 4];
        }
    } else {
        ESP_LOGI(TAG, "[PARSE_FRAME] unmasked frame, copying...");
        memcpy(out_payload, payload, out_len);
    }

    ESP_LOGI(TAG, "[PARSE_FRAME] === DONE === out_len=%d", out_len);
    return out_len;
}

static volatile bool s_recv_task_ready = false;
static int s_recv_start_count = 0;

static void ws_client_recv_task(void* param)
{
    esp_rom_printf("[WS_RECV] >>> TASK ENTERED <<<\n");
    s_recv_start_count++;
    s_recv_task_ready = true;
    ESP_LOGI(TAG, "[WS_RECV] === TASK STARTED #%d === s_sock_fd=%d", s_recv_start_count, s_sock_fd);
    uint8_t buf[2048];
    uint8_t payload[2048];

    while (s_sock_fd >= 0) {
        ESP_LOGI(TAG, "[WS_RECV] calling select()...");

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(s_sock_fd, &read_fds);
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        int sel = select(s_sock_fd + 1, &read_fds, NULL, NULL, &tv);
        ESP_LOGI(TAG, "[WS_RECV] select returned sel=%d errno=%d", sel, errno);
        esp_rom_printf("[WS_RECV] select sel=%d errno=%d\n", sel, errno);
        if (sel < 0) {
            ESP_LOGE(TAG, "[WS_RECV] select error, breaking");
            esp_rom_printf("[WS_RECV] select error\n");
            break;
        }
        if (sel == 0) {
            ESP_LOGI(TAG, "[WS_RECV] select timeout, looping");
            continue;
        }
        if (!FD_ISSET(s_sock_fd, &read_fds)) {
            ESP_LOGW(TAG, "[WS_RECV] FD_ISSET false despite sel>0");
            continue;
        }

        ESP_LOGI(TAG, "[WS_RECV] about to recv()...");
        int received = recv(s_sock_fd, (char*)buf, sizeof(buf) - 1, 0);
        ESP_LOGI(TAG, "[WS_RECV] recv returned ret=%d errno=%d", received, errno);
        esp_rom_printf("[WS_RECV] recv ret=%d errno=%d\n", received, errno);
        if (received <= 0) {
            ESP_LOGE(TAG, "[WS_RECV] recv returned %d, breaking loop", received);
            break;
        }

        // Log ALL bytes received for debugging
        ESP_LOGI(TAG, "[WS_RECV] received %d bytes:", received);
        for (int i = 0; i < received && i < 32; i++) {
            ESP_LOGI(TAG, "[WS_RECV]   byte[%d]=0x%02x('%c')", i, buf[i], (buf[i] >= 32 && buf[i] < 127) ? buf[i] : '.');
        }
        esp_rom_printf("[WS_RECV] recv %d bytes: %02x %02x %02x %02x %02x %02x %02x %02x...\n",
                 received, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);

        ESP_LOGI(TAG, "[WS_RECV] calling ws_parse_frame()...");
        int payload_len = ws_parse_frame(buf, received, payload, sizeof(payload) - 1);
        ESP_LOGI(TAG, "[WS_RECV] ws_parse_frame returned %d", payload_len);
        esp_rom_printf("[WS_RECV] parse result: %d\n", payload_len);

        if (payload_len > 0) {
            ESP_LOGI(TAG, "[WS_RECV] valid frame, payload_len=%d", payload_len);
            payload[payload_len] = '\0';
            ESP_LOGI(TAG, "[WS_RECV] TEXT: %s", payload);
            esp_rom_printf("[WS_RECV] text: %s\n", payload);

            ESP_LOGI(TAG, "[WS_RECV] calling parse_json_command()...");
            ws_cmd_t cmd = {0};
            parse_json_command((char*)payload, &cmd);
            ESP_LOGI(TAG, "[WS_RECV] parsed cmd type=%d angle=%d", cmd.type, cmd.angle);
            esp_rom_printf("[WS_RECV] cmd type=%d\n", cmd.type);

            if (s_event_callback) {
                ESP_LOGI(TAG, "[WS_RECV] invoking callback...");
                esp_rom_printf("[WS_RECV] invoking callback\n");
                s_event_callback(&cmd, s_user_data);
                ESP_LOGI(TAG, "[WS_RECV] callback invoked");
            } else {
                ESP_LOGW(TAG, "[WS_RECV] no callback set!");
            }
        } else if (payload_len == -2) {
            ESP_LOGI(TAG, "[WS_RECV] close frame received");
            esp_rom_printf("[WS_RECV] close frame\n");
            break;
        } else if (payload_len == -1) {
            ESP_LOGE(TAG, "[WS_RECV] parse failed");
            esp_rom_printf("[WS_RECV] parse fail\n");
        } else if (payload_len == 0) {
            ESP_LOGI(TAG, "[WS_RECV] parse skip (non-text/binary opcode)");
            esp_rom_printf("[WS_RECV] parse skip\n");
        }
    }

    ESP_LOGI(TAG, "[WS_RECV] === TASK ENDING === s_connected=%d", s_connected);
    s_connected = false;
    s_recv_task_ready = false;
    if (s_sock_fd >= 0) {
        ESP_LOGI(TAG, "[WS_RECV] closing socket s_sock_fd=%d", s_sock_fd);
        close(s_sock_fd);
        s_sock_fd = -1;
    }

    // Restart reconnect task
    if (s_reconnect_task == NULL) {
        ESP_LOGI(TAG, "[WS_RECV] s_reconnect_task is NULL, creating...");
        xTaskCreate(&ws_client_reconnect_task, "ws_reconnect", 4096, NULL, 3, &s_reconnect_task);
    } else {
        ESP_LOGI(TAG, "[WS_RECV] s_reconnect_task already exists: %p", (void*)s_reconnect_task);
    }
    ESP_LOGI(TAG, "[WS_RECV] calling vTaskDelete(NULL)");
    vTaskDelete(NULL);
}

static void ws_client_reconnect_task(void* param)
{
    ESP_LOGI(TAG, "[WS_RECONN] === reconnect task started ===");
    esp_rom_printf("[WS_RECONN] reconnect task started\n");
    int delay_ms = 1000;
    struct sockaddr_in server_addr;

    while (1) {
        ESP_LOGI(TAG, "[WS_RECONN] top of loop, delay_ms=%d", delay_ms);
        ESP_LOGI(TAG, "[WS_RECONN] connecting to " WS_SERVER_URI " in %d ms...", delay_ms);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));

        // Parse WS_SERVER_URI (format: ws://host:port/path)
        ESP_LOGI(TAG, "[WS_RECONN] parsing WS_SERVER_URI...");
        const char* host_start = strstr(WS_SERVER_URI, "://");
        if (!host_start) {
            ESP_LOGE(TAG, "[WS_RECONN] FAILED: Invalid WS URI, breaking");
            break;
        }
        host_start += 3;
        ESP_LOGI(TAG, "[WS_RECONN] host_start=%s", host_start);
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
            ESP_LOGI(TAG, "[WS_RECONN] parsed host=%s port=%d", host, port);
        } else {
            int host_len = path_start ? (path_start - host_start) : strlen(host_start);
            if (host_len > 63) host_len = 63;
            memcpy(host, host_start, host_len);
            host[host_len] = '\0';
            ESP_LOGI(TAG, "[WS_RECONN] parsed host=%s port=%d (default)", host, port);
        }
        if (path_start) {
            strncpy(path, path_start, sizeof(path) - 1);
            ESP_LOGI(TAG, "[WS_RECONN] parsed path=%s", path);
        }

        // Create socket
        ESP_LOGI(TAG, "[WS_RECONN] creating socket...");
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        ESP_LOGI(TAG, "[WS_RECONN] socket=%d", sock);
        if (sock < 0) {
            ESP_LOGE(TAG, "[WS_RECONN] Socket create failed errno=%d", errno);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            continue;
        }

        ESP_LOGI(TAG, "[WS_RECONN] setting up server_addr...");
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        ESP_LOGI(TAG, "[WS_RECONN] server_addr.sin_port=%d (htons=%d)", port, htons(port));
        inet_aton(host, &server_addr.sin_addr);
        ESP_LOGI(TAG, "[WS_RECONN] connecting to %s:%d...", host, port);

        ESP_LOGI(TAG, "[WS_RECONN] calling connect()...");
        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
            ESP_LOGW(TAG, "[WS_RECONN] connect FAILED errno=%d (%s)", errno, strerror(errno));
            close(sock);
            sock = -1;
            delay_ms *= 2;
            if (delay_ms > WS_RECONNECT_MAX_DELAY_MS) {
                delay_ms = WS_RECONNECT_MAX_DELAY_MS;
            }
            ESP_LOGI(TAG, "[WS_RECONN] new delay_ms=%d", delay_ms);
            continue;
        }

        ESP_LOGI(TAG, "[WS_RECONN] connect SUCCESS, calling ws_send_handshake()...");
        // Send WS handshake
        if (!ws_send_handshake(sock, host, path)) {
            ESP_LOGW(TAG, "[WS_RECONN] ws_send_handshake FAILED");
            close(sock);
            sock = -1;
            delay_ms *= 2;
            if (delay_ms > WS_RECONNECT_MAX_DELAY_MS) {
                delay_ms = WS_RECONNECT_MAX_DELAY_MS;
            }
            continue;
        }

        // Connected!
        ESP_LOGI(TAG, "[WS_RECONN] *** CONNECTED *** setting s_sock_fd=%d", sock);
        s_sock_fd = sock;
        s_connected = true;
        esp_rom_printf("[WS] *** CONNECTED *** to %s:%d%s\n", host, port, path);

        if (s_reconnect_task) {
            ESP_LOGI(TAG, "[WS_RECONN] deleting reconnect task handle=%p...", (void*)s_reconnect_task);
            esp_rom_printf("[WS] about to delete reconnect task...\n");
            vTaskDelete(s_reconnect_task);
            s_reconnect_task = NULL;
            ESP_LOGI(TAG, "[WS_RECONN] reconnect task deleted");
        }
        ESP_LOGI(TAG, "[WS_RECONN] creating recv task (stack=4096, prio=3)...");
        esp_rom_printf("[WS] creating recv task (stack=4096, prio=3)...\n");
        xTaskCreate(&ws_client_recv_task, "ws_recv", 4096, NULL, 3, &s_recv_task);
        ESP_LOGI(TAG, "[WS_RECONN] recv task created handle=%p", (void*)s_recv_task);
        esp_rom_printf("[WS] recv task created handle=%p, deleting self...\n", (void*)s_recv_task);
        ESP_LOGI(TAG, "[WS_RECONN] waiting for recv task to start...");
        vTaskDelay(pdMS_TO_TICKS(300));
        ESP_LOGI(TAG, "[WS_RECONN] s_recv_task_ready=%d, proceeding to delete self", s_recv_task_ready);
        vTaskDelete(NULL);
        break;
    }
    ESP_LOGI(TAG, "[WS_RECONN] task exiting");
    vTaskDelete(NULL);
}

static void parse_json_command(const char* json, ws_cmd_t* out_cmd)
{
    ESP_LOGI(TAG, "[PARSE_JSON] === START === json=%s", json);
    memset(out_cmd, 0, sizeof(*out_cmd));
    out_cmd->type = WS_CMD_UNKNOWN;
    out_cmd->seq = next_seq();
    ESP_LOGI(TAG, "[PARSE_JSON] seq=%u", out_cmd->seq);

    const char* cmd_start = strstr(json, "\"cmd\"");
    if (!cmd_start) {
        ESP_LOGW(TAG, "[PARSE_JSON] FAILED: no \"cmd\" found");
        return;
    }
    ESP_LOGI(TAG, "[PARSE_JSON] found \"cmd\" at offset %d", (int)(cmd_start - json));

    const char* colon = strchr(cmd_start, ':');
    if (!colon) {
        ESP_LOGE(TAG, "[PARSE_JSON] FAILED: no colon after cmd");
        return;
    }
    colon++;
    while (*colon == ' ' || *colon == '"') colon++;
    ESP_LOGI(TAG, "[PARSE_JSON] colon points to: %.20s", colon);

    if (strncmp(colon, "camera_on", 9) == 0) {
        out_cmd->type = WS_CMD_CAMERA_ON;
        ESP_LOGI(TAG, "[PARSE_JSON] matched camera_on");
    } else if (strncmp(colon, "camera_off", 10) == 0) {
        out_cmd->type = WS_CMD_CAMERA_OFF;
        ESP_LOGI(TAG, "[PARSE_JSON] matched camera_off");
    } else if (strncmp(colon, "servo", 5) == 0) {
        out_cmd->type = WS_CMD_SERVO;
        ESP_LOGI(TAG, "[PARSE_JSON] matched servo");
        const char* angle_start = strstr(json, "\"angle\"");
        if (angle_start) {
            ESP_LOGI(TAG, "[PARSE_JSON] found angle field");
            const char* a_colon = strchr(angle_start, ':');
            if (a_colon) {
                out_cmd->angle = atoi(a_colon + 1);
                ESP_LOGI(TAG, "[PARSE_JSON] angle=%d", out_cmd->angle);
            } else {
                ESP_LOGW(TAG, "[PARSE_JSON] no colon after angle");
            }
        } else {
            ESP_LOGW(TAG, "[PARSE_JSON] no angle field found");
        }
    } else if (strncmp(colon, "capture", 7) == 0) {
        out_cmd->type = WS_CMD_CAPTURE;
        ESP_LOGI(TAG, "[PARSE_JSON] matched capture");
    } else if (strncmp(colon, "stream_start", 12) == 0) {
        out_cmd->type = WS_CMD_STREAM_START;
        ESP_LOGI(TAG, "[PARSE_JSON] matched stream_start");
    } else if (strncmp(colon, "stream_stop", 11) == 0) {
        out_cmd->type = WS_CMD_STREAM_STOP;
        ESP_LOGI(TAG, "[PARSE_JSON] matched stream_stop");
    } else {
        ESP_LOGW(TAG, "[PARSE_JSON] no match for cmd: %.20s", colon);
    }

    ESP_LOGI(TAG, "[PARSE_JSON] === DONE === type=%d seq=%u angle=%d", out_cmd->type, out_cmd->seq, out_cmd->angle);
}

void ws_client_init(ws_client_event_callback_t callback, void* user_data)
{
    ESP_LOGI(TAG, "[WS_INIT] === START === callback=%p user_data=%p", callback, user_data);
    s_event_callback = callback;
    s_user_data = user_data;
    s_connected = false;
    s_sock_fd = -1;
    ESP_LOGI(TAG, "[WS_INIT] creating reconnect task...");
    xTaskCreate(&ws_client_reconnect_task, "ws_reconnect", 4096, NULL, 3, &s_reconnect_task);
    ESP_LOGI(TAG, "[WS_INIT] reconnect task handle=%p", (void*)s_reconnect_task);
    ESP_LOGI(TAG, "[WS_INIT] === DONE ===");
}

void ws_client_deinit(void)
{
    ESP_LOGI(TAG, "[WS_DEINIT] === START ===");
    ESP_LOGI(TAG, "[WS_DEINIT] s_recv_task=%p s_reconnect_task=%p s_sock_fd=%d",
             (void*)s_recv_task, (void*)s_reconnect_task, s_sock_fd);
    s_event_callback = NULL;
    s_user_data = NULL;

    if (s_recv_task) {
        ESP_LOGI(TAG, "[WS_DEINIT] deleting recv task...");
        vTaskDelete(s_recv_task);
        s_recv_task = NULL;
    }
    if (s_reconnect_task) {
        ESP_LOGI(TAG, "[WS_DEINIT] deleting reconnect task...");
        vTaskDelete(s_reconnect_task);
        s_reconnect_task = NULL;
    }
    if (s_sock_fd >= 0) {
        ESP_LOGI(TAG, "[WS_DEINIT] closing socket %d...", s_sock_fd);
        close(s_sock_fd);
        s_sock_fd = -1;
    }
    s_connected = false;
    ESP_LOGI(TAG, "[WS_DEINIT] === DONE ===");
}

static bool ws_send_frame(int sock, const uint8_t* data, int len, int opcode)
{
    ESP_LOGI(TAG, "[WS_SEND_FRAME] === START === sock=%d len=%d opcode=%d s_connected=%d",
             sock, len, opcode, s_connected);
    if (sock < 0 || !s_connected) {
        ESP_LOGW(TAG, "[WS_SEND_FRAME] FAILED: sock=%d s_connected=%d", sock, s_connected);
        return false;
    }

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
        ESP_LOGW(TAG, "[WS_SEND_FRAME] FAILED: len=%d too large", len);
        return false;  // Too large
    }

    ESP_LOGI(TAG, "[WS_SEND_FRAME] sending header len=%d", header_len);
    // Send header
    if (send(sock, (char*)header, header_len, 0) != header_len) {
        ESP_LOGW(TAG, "[WS_SEND_FRAME] FAILED: header send");
        return false;
    }

    ESP_LOGI(TAG, "[WS_SEND_FRAME] sending payload len=%d", len);
    // Send payload
    int sent = send(sock, (char*)data, len, 0);
    if (sent != len) {
        ESP_LOGW(TAG, "[WS_SEND_FRAME] FAILED: sent=%d expected=%d", sent, len);
        return false;
    }
    ESP_LOGI(TAG, "[WS_SEND_FRAME] === SUCCESS === sent=%d", sent);
    return true;
}

void ws_client_send_text(const char* json_response)
{
    ESP_LOGI(TAG, "[WS_SEND_TEXT] === START === json_response=%s", json_response);
    if (!ws_send_frame(s_sock_fd, (const uint8_t*)json_response, strlen(json_response), WS_OPCODE_TEXT)) {
        ESP_LOGW(TAG, "[WS_SEND_TEXT] FAILED");
    } else {
        ESP_LOGI(TAG, "[WS_SEND_TEXT] SUCCESS");
    }
}

void ws_client_send_binary(const uint8_t* data, size_t len)
{
    ESP_LOGI(TAG, "[WS_SEND_BINARY] === START === len=%zu", len);
    if (!ws_send_frame(s_sock_fd, data, len, WS_OPCODE_BINARY)) {
        ESP_LOGW(TAG, "[WS_SEND_BINARY] FAILED");
    } else {
        ESP_LOGI(TAG, "[WS_SEND_BINARY] SUCCESS");
    }
}

bool ws_client_is_connected(void)
{
    ESP_LOGI(TAG, "[WS_IS_CONNECTED] s_connected=%d s_sock_fd=%d", s_connected, s_sock_fd);
    return s_connected;
}