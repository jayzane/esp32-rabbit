#include "ws_control.h"
#include "camera_ctrl.h"
#include "servo.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_http_server.h"

static const char *TAG = "ws_control";

#define CONTROL_PATH "/control"

static ws_control_callback_t g_callback = NULL;
static void *g_user_data = NULL;
static httpd_handle_t g_httpd_handle = NULL;

static esp_err_t control_get_handler(httpd_req_t *req)
{
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"status\":\"ok\",\"camera\":\"%s\"}",
        camera_ctrl_is_on() ? "on" : "off");
    httpd_resp_send(req, buf, strlen(buf));
    return ESP_OK;
}

static esp_err_t control_post_handler(httpd_req_t *req)
{
    char buf[512];
    memset(buf, 0, sizeof(buf));

    // Truncate if needed
    size_t buf_len = sizeof(buf) - 1;
    if (req->content_len < buf_len) {
        buf_len = req->content_len;
    }

    int ret = httpd_req_recv(req, buf, buf_len);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive request");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    ESP_LOGI(TAG, "Received: %s", buf);

    // Parse JSON looking for "action" field
    ws_action_t action = WS_ACTION_STATUS;
    char *action_start = strstr(buf, "\"action\"");
    if (action_start) {
        action_start = strchr(action_start, ':');
        if (action_start) {
            action_start++;
            // Skip whitespace and quotes
            while (*action_start == ' ' || *action_start == '"') {
                action_start++;
            }
            if (strncmp(action_start, "on", 2) == 0) {
                action = WS_ACTION_ON;
            } else if (strncmp(action_start, "off", 3) == 0) {
                action = WS_ACTION_OFF;
            } else if (strncmp(action_start, "servo", 5) == 0) {
                action = WS_ACTION_SERVO;
            } else {
                action = WS_ACTION_STATUS;
            }
        }
    }

    ESP_LOGI(TAG, "Parsed action: %d", action);

    // Handle servo action directly (no callback support for angle)
    if (action == WS_ACTION_SERVO) {
        // Parse "angle" field from JSON
        int angle = -1;
        char *angle_start = strstr(buf, "\"angle\"");
        if (angle_start) {
            angle_start = strchr(angle_start, ':');
            if (angle_start) {
                angle_start++;
                while (*angle_start == ' ' || *angle_start == '"') {
                    angle_start++;
                }
                angle = atoi(angle_start);
            }
        }

        // Validate angle range
        if (angle < 0 || angle > 180) {
            snprintf(buf, sizeof(buf),
                "{\"status\":\"error\",\"reason\":\"angle_out_of_range\"}");
        } else {
            servo_set_angle(angle);
            snprintf(buf, sizeof(buf),
                "{\"status\":\"ok\",\"angle\":%d}", angle);
        }
        httpd_resp_send(req, buf, strlen(buf));
        return ESP_OK;
    }

    // Invoke stored callback with parsed action
    if (g_callback) {
        g_callback(action, g_user_data);
    }

    // Send response
    snprintf(buf, sizeof(buf),
        "{\"status\":\"ok\",\"camera\":\"%s\"}",
        camera_ctrl_is_on() ? "on" : "off");
    httpd_resp_send(req, buf, strlen(buf));

    return ESP_OK;
}

static esp_err_t root_handler(httpd_req_t *req)
{
    const char *resp = "<html><head><title>ESP32 Camera</title></head>"
        "<body><h1>ESP32 Camera Surveillance</h1>"
        "<p>Camera is <span id='status'>...</span></p>"
        "<p><button onclick='fetch(\"/control\",{method:\"POST\",headers:{\"Content-Type\":\"application/json\"},body:JSON.stringify({action:\"on\"})}).then(r=>r.json()).then(d=>document.getElementById(\"status\").innerText=d.camera)}'>ON</button>"
        "<button onclick='fetch(\"/control\",{method:\"POST\",headers:{\"Content-Type\":\"application/json\"},body:JSON.stringify({action:\"off\"})}).then(r=>r.json()).then(d=>document.getElementById(\"status\").innerText=d.camera)}'>OFF</button></p>"
        "<img src='/stream' /><br/>"
        "<script>fetch(\"/control\").then(r=>r.json()).then(d=>document.getElementById(\"status\").innerText=d.camera);</script>"
        "</body></html>";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

void ws_control_init(ws_control_callback_t callback, void *user_data)
{
    g_callback = callback;
    g_user_data = user_data;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8080;
    config.ctrl_port = 8080;

    httpd_uri_t control_get_uri = {
        .uri = CONTROL_PATH,
        .method = HTTP_GET,
        .handler = control_get_handler,
        .user_ctx = NULL
    };

    httpd_uri_t control_post_uri = {
        .uri = CONTROL_PATH,
        .method = HTTP_POST,
        .handler = control_post_handler,
        .user_ctx = NULL
    };

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL
    };

    if (httpd_start(&g_httpd_handle, &config) == ESP_OK) {
        httpd_register_uri_handler(g_httpd_handle, &control_get_uri);
        httpd_register_uri_handler(g_httpd_handle, &control_post_uri);
        httpd_register_uri_handler(g_httpd_handle, &root_uri);
        ESP_LOGI(TAG, "HTTP control server started on port 8080");
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP control server");
        g_httpd_handle = NULL;
    }
}

void ws_control_deinit(void)
{
    if (g_httpd_handle) {
        httpd_stop(g_httpd_handle);
        g_httpd_handle = NULL;
    }
    g_callback = NULL;
    g_user_data = NULL;
    ESP_LOGI(TAG, "HTTP control server deinitialized");
}

void ws_control_send_response(const char *json_response)
{
    if (g_httpd_handle == NULL || json_response == NULL) {
        ESP_LOGW(TAG, "Cannot send response: no active connection or null response");
        return;
    }
    ESP_LOGI(TAG, "Response ready: %s", json_response);
}
