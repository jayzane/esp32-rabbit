#include "camera_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "camera_driver.h"

static const char* TAG = "camera_server";
static httpd_handle_t s_httpd = NULL;

static esp_err_t stream_handler(httpd_req_t* req) {
    char part_buf[128];
    esp_err_t res = ESP_OK;

    if (!camera_driver_is_running()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Camera not running");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");

    while (true) {
        camera_fb_t* fb = camera_driver_get_frame();
        if (!fb) {
            ESP_LOGW(TAG, "Frame capture timeout");
            res = ESP_FAIL;
            break;
        }

        if (fb->format == PIXFORMAT_JPEG) {
            size_t hlen = snprintf(part_buf, sizeof(part_buf),
                "--frame\r\n"
                "Content-Type: image/jpeg\r\n"
                "Content-Length: %u\r\n\r\n",
                (unsigned int)fb->len);

            res = httpd_resp_send_chunk(req, part_buf, hlen);
            if (res == ESP_OK) {
                res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
            }
        }

        camera_driver_return_frame(fb);

        if (res != ESP_OK) break;

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return res;
}

static esp_err_t index_handler(httpd_req_t* req) {
    const char* resp = "<html><head><title>ESP32 Camera</title></head>"
        "<body><h1>ESP32 Camera Stream</h1>"
        "<img src=\"/stream\" /></body></html>";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

void camera_server_start(void) {
    if (s_httpd) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8081;

    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL
    };

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = NULL
    };

    if (httpd_start(&s_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(s_httpd, &stream_uri);
        httpd_register_uri_handler(s_httpd, &index_uri);
        ESP_LOGI(TAG, "Camera server started on port 8081");
    } else {
        ESP_LOGE(TAG, "Failed to start camera server");
    }
}

void camera_server_stop(void) {
    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
        ESP_LOGI(TAG, "Camera server stopped");
    }
}

bool camera_server_is_running(void) {
    return s_httpd != NULL;
}