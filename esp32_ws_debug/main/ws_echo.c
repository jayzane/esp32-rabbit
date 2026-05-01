#include "ws_echo.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_crypto_hash.h"
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

static const char* TAG = "ws_echo";

// Server config - parsed from WS_ECHO_SERVER_URI
static char s_host[64] = {0};
static int  s_port = 11081;
static char s_path[64] = "/";

// Connection state
static ws_echo_state_t s_state = WS_ECHO_DISCONNECTED;
static int s_sock_fd = -1;
static ws_echo_on_receive_t s_recv_cb = NULL;

// Heartbeat
static uint32_t s_seq = 0;
static int64_t s_last_heartbeat_ms = 0;
static TaskHandle_t s_task_handle = NULL;
static bool s_task_running = false;

// ---------------------------------------------------------------------------
// SHA1 for WS handshake - uses mbedTLS via esp_crypto_hash
// ---------------------------------------------------------------------------

static bool sha1_raw(const char* data, size_t len, uint8_t* out_sha1_20bytes)
{
    // mbedTLS SHA-1: supported in ESP-IDF via mbedtls library
    // We implement a minimal portable SHA-1 since mbedTLS API varies by IDF version
    // Use the same approach as ESP-IDF examples (HAL SHA1 or mbedtls_md)
    // For this debug project, use mbedTLS md_context approach

    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA1;
    const mbedtls_md_info_t* md_info;

    mbedtls_md_init(&ctx);
    md_info = mbedtls_md_info_from_type(md_type);
    if (!md_info) {
        ESP_LOGE(TAG, "SHA1 info not found");
        mbedtls_md_free(&ctx);
        return false;
    }

    int ret = mbedtls_md_setup(&ctx, md_info, 0);
    if (ret != 0) {
        ESP_LOGE(TAG, "SHA1 setup failed: %d", ret);
        mbedtls_md_free(&ctx);
        return false;
    }

    mbedtls_md_update(&ctx, (const unsigned char*)data, len);
    mbedtls_md_finish(&ctx, out_sha1_20bytes);
    mbedtls_md_free(&ctx);

    return true;
}

// Base64 encode (standard table)
static const char s_base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_encode(const uint8_t* in, int in_len, char* out)
{
    int i, j;
    for (i = 0, j = 0; i < in_len; i += 3) {
        int a = in[i];
        int b = (i + 1 < in_len) ? in[i + 1] : 0;
        int c = (i + 2 < in_len) ? in[i + 2] : 0;
        int triplet = (a << 16) | (b << 8) | c;
        out[j++] = s_base64_table[(triplet >> 18) & 0x3F];
        out[j++] = s_base64_table[(triplet >> 12) & 0x3F];
        out[j++] = (i + 1 < in_len) ? s_base64_table[(triplet >> 6) & 0x3F] : '=';
        out[j++] = (i + 2 < in_len) ? s_base64_table[triplet & 0x3F] : '=';
    }
    out[j] = '\0';
    return j;
}

// ---------------------------------------------------------------------------
// URI parsing
// ---------------------------------------------------------------------------

static void parse_uri(void)
{
    const char* uri = WS_ECHO_SERVER_URI;
    // Skip "ws://"
    const char* p = strstr(uri, "://");
    if (!p) return;
    p += 3;

    // host:port/path
    const char* colon = strchr(p, ':');
    const char* slash = strchr(p, '/');

    if (colon && (!slash || colon < slash)) {
        int host_len = colon - p;
        if (host_len > 63) host_len = 63;
        memcpy(s_host, p, host_len);
        s_host[host_len] = '\0';
        s_port = atoi(colon + 1);
    } else {
        int host_len = slash ? (slash - p) : strlen(p);
        if (host_len > 63) host_len = 63;
        memcpy(s_host, p, host_len);
        s_host[host_len] = '\0';
    }

    if (slash) {
        strncpy(s_path, slash, sizeof(s_path) - 1);
        s_path[sizeof(s_path) - 1] = '\0';
    }

    ESP_LOGI(TAG, "URI parsed: host=%s port=%d path=%s", s_host, s_port, s_path);
}

// ---------------------------------------------------------------------------
// WS Frame parse (server->client, unmasked)
// ---------------------------------------------------------------------------

static int ws_parse_frame(const uint8_t* data, int len, uint8_t* out_payload, int max_len)
{
    if (len < 2) return -1;

    int opcode = data[0] & 0x0F;
    int payload_len = data[1] & 0x7F;
    int header_len = 2;
    int mask_bit = (data[1] & 0x80) >> 7;

    if (payload_len == 126) {
        if (len < 4) return -1;
        payload_len = (data[2] << 8) | data[3];
        header_len = 4;
    } else if (payload_len == 127) {
        // Skip >64KB frames for debug
        return -1;
    }

    // Server frames are NOT masked (mask_bit should be 0)
    // But we handle masked anyway for robustness
    int mask_len = mask_bit ? 4 : 0;
    if (len < header_len + mask_len + payload_len) {
        return -1;
    }

    const uint8_t* payload = data + header_len + mask_len;

    if (opcode == 0x08) {
        // Close frame
        return -2;
    }
    if (opcode != 0x01 && opcode != 0x02) {
        // Ignore non-text/binary
        return 0;
    }

    int out_len = payload_len;
    if (out_len > max_len) out_len = max_len;

    if (mask_bit) {
        const uint8_t* mask = data + header_len;
        for (int i = 0; i < out_len; i++) {
            out_payload[i] = payload[i] ^ mask[i % 4];
        }
    } else {
        memcpy(out_payload, payload, out_len);
    }

    return out_len;
}

// ---------------------------------------------------------------------------
// WS Send frame (client->server, ALWAYS masked per RFC 6455)
// ---------------------------------------------------------------------------

static bool ws_send_frame(int sock, const uint8_t* data, int len, int opcode)
{
    if (sock < 0) return false;

    // Mask bit = 1 for client frames
    uint8_t header[10];
    int header_len = 2;

    header[0] = 0x80 | opcode;  // FIN + opcode
    header[1] = 0x80 | (len & 0x7F);  // Mask bit set

    // Generate 4-byte mask key
    uint8_t mask_key[4];
    for (int i = 0; i < 4; i++) {
        mask_key[i] = (uint8_t)(esp_random() & 0xFF);
    }

    // Add mask key to header for length < 126
    if (len < 126) {
        memcpy(header + 2, mask_key, 4);
        header_len = 6;
    } else {
        // 126 or 127 length
        header[1] = 0x80 | 126;
        header[2] = (len >> 8) & 0xFF;
        header[3] = len & 0xFF;
        memcpy(header + 4, mask_key, 4);
        header_len = 8;
    }

    // Send header
    if (send(sock, (char*)header, header_len, 0) != header_len) {
        return false;
    }

    // Send masked payload
    uint8_t masked[2048];
    int masked_len = (len > 2048) ? 2048 : len;
    for (int i = 0; i < masked_len; i++) {
        masked[i] = data[i] ^ mask_key[i % 4];
    }

    int sent = send(sock, (char*)masked, masked_len, 0);
    return (sent == masked_len);
}

// ---------------------------------------------------------------------------
// WS Handshake
// ---------------------------------------------------------------------------

static bool ws_handshake(int sock)
{
    // Generate 16-byte random key
    uint8_t key_bin[16];
    for (int i = 0; i < 16; i++) {
        key_bin[i] = (uint8_t)(esp_random() & 0xFF);
    }

    char key_b64[32];
    base64_encode(key_bin, 16, key_b64);

    // Send HTTP Upgrade request
    char request[512];
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        s_path, s_host, s_port, key_b64);

    ESP_LOGI(TAG, "[HS] Sending handshake...");
    if (send(sock, request, strlen(request), 0) < 0) {
        ESP_LOGE(TAG, "[HS] send failed");
        return false;
    }

    // Read response
    char response[1024];
    int received = recv(sock, response, sizeof(response) - 1, 0);
    if (received <= 0) {
        ESP_LOGE(TAG, "[HS] recv failed ret=%d", received);
        return false;
    }
    response[received] = '\0';

    ESP_LOGI(TAG, "[HS] Response:\n%s", response);

    // Verify 101 Switching Protocols
    if (strstr(response, "101") == NULL) {
        ESP_LOGE(TAG, "[HS] No 101 in response!");
        return false;
    }

    ESP_LOGI(TAG, "[HS] SUCCESS");
    return true;
}

// ---------------------------------------------------------------------------
// Send heartbeat
// ---------------------------------------------------------------------------

static void send_heartbeat(void)
{
    if (s_sock_fd < 0) return;

    s_seq++;
    char msg[128];
    int len = snprintf(msg, sizeof(msg),
        "{\"seq\":%u,\"type\":\"heartbeat\",\"source\":\"esp32\"}", s_seq);

    if (ws_send_frame(s_sock_fd, (uint8_t*)msg, len, 0x01)) {
        ESP_LOGI(TAG, "[HB] Sent heartbeat seq=%u", s_seq);
    } else {
        ESP_LOGE(TAG, "[HB] Send failed");
    }
}

// ---------------------------------------------------------------------------
// Main task
// ---------------------------------------------------------------------------

static void ws_echo_task(void* param)
{
    uint8_t buf[4096];
    uint8_t payload[4096];
    int64_t next_reconnect_ms = 0;
    int reconnect_delay_ms = 1000;

    parse_uri();

    ESP_LOGI(TAG, "[TASK] Starting. Target: %s:%d%s", s_host, s_port, s_path);

    while (s_task_running) {
        // ---- Connect phase ----
        if (s_state == WS_ECHO_DISCONNECTED) {
            int64_t now = esp_timer_get_time() / 1000;
            if (now < next_reconnect_ms) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            ESP_LOGI(TAG, "[TASK] Connecting to %s:%d...", s_host, s_port);
            s_state = WS_ECHO_CONNECTING;

            // Create socket
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                ESP_LOGE(TAG, "[TASK] socket() failed errno=%d", errno);
                goto reconnect_delay;
            }

            // Resolve host
            struct hostent* he = gethostbyname(s_host);
            if (!he) {
                ESP_LOGE(TAG, "[TASK] gethostbyname(%s) failed", s_host);
                close(sock);
                goto reconnect_delay;
            }

            // Connect
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(s_port);
            memcpy(&addr.sin_addr, he->h_addr, he->h_length);

            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
                ESP_LOGE(TAG, "[TASK] connect() failed errno=%d", errno);
                close(sock);
                goto reconnect_delay;
            }

            ESP_LOGI(TAG, "[TASK] Connected, doing WS handshake...");
            if (!ws_handshake(sock)) {
                ESP_LOGE(TAG, "[TASK] WS handshake failed");
                close(sock);
                goto reconnect_delay;
            }

            ESP_LOGI(TAG, "[TASK] *** WS CONNECTED ***");
            s_sock_fd = sock;
            s_state = WS_ECHO_CONNECTED;
            reconnect_delay_ms = 1000;
            s_last_heartbeat_ms = esp_timer_get_time() / 1000;

            if (s_recv_cb) {
                // Signal connected (via log, no callback needed for debug)
            }

            continue;

reconnect_delay:
            s_state = WS_ECHO_DISCONNECTED;
            next_reconnect_ms = (esp_timer_get_time() / 1000) + reconnect_delay_ms;
            reconnect_delay_ms *= 2;
            if (reconnect_delay_ms > WS_ECHO_RECONNECT_MAX_MS) {
                reconnect_delay_ms = WS_ECHO_RECONNECT_MAX_MS;
            }
            ESP_LOGI(TAG, "[TASK] Reconnecting in %d ms...", reconnect_delay_ms);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // ---- Connected: select + recv ----
        if (s_state == WS_ECHO_CONNECTED) {
            int64_t now_ms = esp_timer_get_time() / 1000;

            // Heartbeat timer
            if (now_ms - s_last_heartbeat_ms >= WS_ECHO_HEARTBEAT_MS) {
                send_heartbeat();
                s_last_heartbeat_ms = now_ms;
            }

            // select with 500ms timeout
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(s_sock_fd, &rfds);
            struct timeval tv = { .tv_sec = 0, .tv_usec = 500000 };

            int sel = select(s_sock_fd + 1, &rfds, NULL, NULL, &tv);
            if (sel < 0) {
                ESP_LOGE(TAG, "[TASK] select error, reconnecting...");
                close(s_sock_fd);
                s_sock_fd = -1;
                s_state = WS_ECHO_DISCONNECTED;
                continue;
            }
            if (sel == 0) {
                continue;
            }
            if (!FD_ISSET(s_sock_fd, &rfds)) {
                continue;
            }

            // Data available
            int received = recv(s_sock_fd, (char*)buf, sizeof(buf) - 1, 0);
            if (received <= 0) {
                ESP_LOGE(TAG, "[TASK] recv returned %d, disconnecting", received);
                close(s_sock_fd);
                s_sock_fd = -1;
                s_state = WS_ECHO_DISCONNECTED;
                continue;
            }

            // Log raw bytes
            ESP_LOGI(TAG, "[TASK] recv %d bytes:", received);
            for (int i = 0; i < received && i < 64; i++) {
                uint8_t c = buf[i];
                ESP_LOGI(TAG, "  [%02x] %c", c, (c >= 32 && c < 127) ? c : '.');
            }

            // Parse WS frames (may be multiple in one recv)
            int pos = 0;
            while (pos < received) {
                int r = ws_parse_frame(buf + pos, received - pos, payload, sizeof(payload) - 1);
                if (r > 0) {
                    payload[r] = '\0';
                    ESP_LOGI(TAG, "[TASK] WS frame text: %s", payload);
                    if (s_recv_cb) {
                        s_recv_cb((char*)payload, r);
                    }
                    pos += 2 + ((received - pos > 2 && (buf[pos + 1] & 0x7F) == 126) ? 4 : 0)
                         + ((buf[pos + 1] & 0x7F) + ((received - pos > 2 && (buf[pos + 1] & 0x7F) >= 126) ? 2 : 0))
                         + (received - pos);
                    // Simple advance for demo (correct implementation would track actual frame size)
                    break;
                } else if (r == -2) {
                    ESP_LOGI(TAG, "[TASK] Close frame received");
                    close(s_sock_fd);
                    s_sock_fd = -1;
                    s_state = WS_ECHO_DISCONNECTED;
                    break;
                } else {
                    // Incomplete or unknown, wait for more data
                    ESP_LOGI(TAG, "[TASK] Partial/invalid frame, waiting for more data");
                    break;
                }
            }
        }
    }

    ESP_LOGI(TAG, "[TASK] Task exiting");
    if (s_sock_fd >= 0) {
        close(s_sock_fd);
        s_sock_fd = -1;
    }
    s_task_running = false;
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ws_echo_init(ws_echo_on_receive_t callback)
{
    ESP_LOGI(TAG, "[INIT] Starting WS echo debug client");
    s_recv_cb = callback;
    s_state = WS_ECHO_DISCONNECTED;
    s_sock_fd = -1;
    s_seq = 0;
    s_task_running = true;
    xTaskCreate(&ws_echo_task, "ws_echo_task", 8192, NULL, 5, &s_task_handle);
    ESP_LOGI(TAG, "[INIT] Task created handle=%p", (void*)s_task_handle);
}

void ws_echo_deinit(void)
{
    ESP_LOGI(TAG, "[DEINIT] Stopping...");
    s_task_running = false;
    if (s_task_handle) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }
    if (s_sock_fd >= 0) {
        close(s_sock_fd);
        s_sock_fd = -1;
    }
    s_state = WS_ECHO_DISCONNECTED;
}

bool ws_echo_is_connected(void)
{
    return s_state == WS_ECHO_CONNECTED;
}

void ws_echo_send(const char* json)
{
    if (s_sock_fd >= 0 && s_state == WS_ECHO_CONNECTED) {
        ws_send_frame(s_sock_fd, (const uint8_t*)json, strlen(json), 0x01);
    }
}
