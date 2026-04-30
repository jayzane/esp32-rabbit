#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_netif_types.h"

#include "camera_ctrl.h"
#include "camera_task.h"
#include "shared_mem.h"
#include "ws_client.h"
#include "servo.h"

static const char* TAG = "app_main";

/* Static IP configuration */
#define ESP32_IP      "10.0.0.110"
#define ESP32_GW      "10.0.0.2"
#define ESP32_NETMASK "255.255.255.0"

static esp_netif_t* s_netif = NULL;

/* WiFi event handler */
/* Periodic status heartbeat task */
static void status_heartbeat_task(void* param)
{
    char status_json[256];
    int heartbeat_count = 0;

    /* Wait for WS client to connect first */
    while (!ws_client_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGI(TAG, "WS connected, starting heartbeat");

    while (ws_client_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(5000));  /* 5 second heartbeat */

        /* Send periodic status with camera and servo state */
        snprintf(status_json, sizeof(status_json),
            "{\"status\":\"heartbeat\",\"seq\":%d,\"camera\":\"%s\",\"servo\":%d}",
            heartbeat_count++,
            camera_ctrl_is_on() ? "on" : "off",
            servo_get_angle());

        ws_client_send_text(status_json);
        ESP_LOGD(TAG, "Heartbeat sent: %s", status_json);
    }

    ESP_LOGW(TAG, "WS disconnected, heartbeat stopped");
    vTaskDelete(NULL);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi STA started, connecting...");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WiFi disconnected, reconnecting...");
                esp_wifi_connect();
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        }
    }
}

/* WebSocket command handling callback */
static void ws_command_callback(ws_cmd_t* cmd, void* user_data)
{
    ESP_LOGI(TAG, "WS command callback: type=%d seq=%u angle=%d", cmd->type, cmd->seq, cmd->angle);
    char response[256];
    int seq = cmd->seq;

    switch (cmd->type) {
        case WS_CMD_CAMERA_ON: {
            if (!camera_ctrl_is_on()) {
                camera_err_t err = camera_ctrl_enable();
                if (err == CAMERA_OK) {
                    snprintf(response, sizeof(response),
                        "{\"seq\":%d,\"status\":\"ok\",\"camera\":\"on\"}", seq);
                } else {
                    snprintf(response, sizeof(response),
                        "{\"seq\":%d,\"status\":\"error\",\"reason\":\"init_failed\"}", seq);
                }
            } else {
                snprintf(response, sizeof(response),
                    "{\"seq\":%d,\"status\":\"ok\",\"camera\":\"on\"}", seq);
            }
            ws_client_send_text(response);
            break;
        }

        case WS_CMD_CAMERA_OFF: {
            if (camera_ctrl_is_on()) {
                camera_ctrl_disable();
            }
            snprintf(response, sizeof(response),
                "{\"seq\":%d,\"status\":\"ok\",\"camera\":\"off\"}", seq);
            ws_client_send_text(response);
            break;
        }

        case WS_CMD_SERVO: {
            int angle = cmd->angle;
            if (angle < 0 || angle > 180) {
                snprintf(response, sizeof(response),
                    "{\"seq\":%d,\"status\":\"error\",\"reason\":\"angle_out_of_range\"}", seq);
            } else {
                servo_set_angle(angle);
                snprintf(response, sizeof(response),
                    "{\"seq\":%d,\"status\":\"ok\",\"angle\":%d}", seq, angle);
            }
            ws_client_send_text(response);
            break;
        }

        case WS_CMD_CAPTURE: {
            size_t frame_len = camera_ctrl_capture();
            if (frame_len > 0) {
                // Take mutex before reading shared JPEG buffer
                if (xSemaphoreTake(s_jpeg_mutex, pdMS_TO_TICKS(3000)) == pdTRUE) {
                    ws_client_send_binary(s_jpeg_buf, frame_len);
                    xSemaphoreGive(s_jpeg_mutex);
                    snprintf(response, sizeof(response),
                        "{\"seq\":%d,\"status\":\"ok\",\"frame_size\":%d}", seq, (int)frame_len);
                    ws_client_send_text(response);
                } else {
                    ESP_LOGE(TAG, "JPEG mutex timeout waiting for frame");
                    snprintf(response, sizeof(response),
                        "{\"seq\":%d,\"status\":\"error\",\"reason\":\"capture_timeout\"}", seq);
                    ws_client_send_text(response);
                }
            } else {
                snprintf(response, sizeof(response),
                    "{\"seq\":%d,\"status\":\"error\",\"reason\":\"capture_failed\"}", seq);
                ws_client_send_text(response);
            }
            break;
        }

        case WS_CMD_STREAM_START:
        case WS_CMD_STREAM_STOP:
            snprintf(response, sizeof(response),
                "{\"seq\":%d,\"status\":\"ok\",\"stream\":\"%s\"}",
                seq, cmd->type == WS_CMD_STREAM_START ? "started" : "stopped");
            ws_client_send_text(response);
            break;

        default:
            snprintf(response, sizeof(response),
                "{\"seq\":%d,\"status\":\"error\",\"reason\":\"unknown_command\"}", seq);
            ws_client_send_text(response);
            break;
    }
}

static void wait_for_wifi_connection(void)
{
    bool connected = false;
    int connect_timeout = 15000;
    int elapsed = 0;

    ESP_LOGI(TAG, "Waiting for WiFi connection...");

    while (!connected && elapsed < connect_timeout) {
        vTaskDelay(pdMS_TO_TICKS(100));
        elapsed += 100;

        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            connected = true;
            ESP_LOGI(TAG, "WiFi connected to: %s", ap_info.ssid);
            break;
        }
    }

    if (!connected) {
        ESP_LOGW(TAG, "WiFi connection timeout, proceeding anyway");
    }
}

static void configure_static_ip(void)
{
    if (!s_netif) return;

    esp_err_t err = esp_netif_dhcpc_stop(s_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGW(TAG, "DHCP stop failed: %s", esp_err_to_name(err));
    }

    esp_netif_ip_info_t ip_info = {0};
    ip_info.ip.addr = esp_ip4addr_aton(ESP32_IP);
    ip_info.netmask.addr = esp_ip4addr_aton(ESP32_NETMASK);
    ip_info.gw.addr = esp_ip4addr_aton(ESP32_GW);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(s_netif, &ip_info));

    esp_netif_dns_info_t dns_info = {0};
    dns_info.ip.type = ESP_IPADDR_TYPE_V4;
    dns_info.ip.u_addr.ip4.addr = esp_ip4addr_aton(ESP32_GW);
    ESP_ERROR_CHECK(esp_netif_set_dns_info(s_netif, ESP_NETIF_DNS_MAIN, &dns_info));

    ESP_LOGI(TAG, "Static IP/GW/DNS configured: " IPSTR,
             IP2STR(&ip_info.ip));
}

void app_main(void)
{
    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* TCP/IP */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* WiFi STA */
    s_netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA_PSK,
            .pmf_cfg = { .capable = true, .required = false },
        },
    };
    strncpy((char*)wifi_config.sta.ssid, CONFIG_ESP_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, CONFIG_ESP_WIFI_PASSWORD, sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Shared memory and queues */
    shared_mem_init();

    /* Initialize servo */
    servo_init();

    /* Initialize camera control (Queue-driven) */
    camera_ctrl_init();

    /* CORE1: start camera task */
    camera_task_start();

    /* WebSocket client */
    ws_client_init(ws_command_callback, NULL);

    /* Start heartbeat task (sends periodic status) */
    xTaskCreate(&status_heartbeat_task, "heartbeat", 4096, NULL, 2, NULL);

    /* Wait for WiFi and configure static IP */
    wait_for_wifi_connection();
    configure_static_ip();

    ESP_LOGI(TAG, "ESP32 dual-core app started. WS client connecting to " WS_SERVER_URI);
}
