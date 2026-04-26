#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "camera_ctrl.h"
#include "camera_server.h"
#include "ws_control.h"

static const char* TAG = "app_main";

/* WiFi event handler */
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
            /* Auto-start camera when WiFi connects */
            camera_ctrl_enable(NULL, NULL);
            camera_server_start();
        }
    }
}

/* WebSocket control callback */
static void ws_control_callback(ws_action_t action, void* user_data)
{
    char response[128];

    switch (action) {
        case WS_ACTION_ON:
            if (!camera_ctrl_is_on()) {
                camera_err_t err = camera_ctrl_enable(NULL, NULL);
                if (err == CAMERA_OK) {
                    snprintf(response, sizeof(response),
                        "{\"status\":\"ok\",\"camera\":\"on\"}");
                } else {
                    snprintf(response, sizeof(response),
                        "{\"status\":\"error\",\"reason\":\"init_failed\"}");
                }
            } else {
                snprintf(response, sizeof(response),
                    "{\"status\":\"ok\",\"camera\":\"on\"}");
            }
            break;

        case WS_ACTION_OFF:
            if (camera_ctrl_is_on()) {
                camera_ctrl_disable();
            }
            snprintf(response, sizeof(response),
                "{\"status\":\"ok\",\"camera\":\"off\"}");
            break;

        case WS_ACTION_STATUS:
        default:
            snprintf(response, sizeof(response),
                "{\"status\":\"ok\",\"camera\":\"%s\"}",
                camera_ctrl_is_on() ? "on" : "off");
            break;
    }

    ws_control_send_response(response);
    ESP_LOGI(TAG, "Camera control: %s", response);
}

void app_main(void)
{
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize camera control */
    camera_ctrl_init();

    /* Initialize WebSocket control with callback */
    ws_control_init(ws_control_callback, NULL);

    /* Initialize TCP/IP */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Create default event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Create default WiFi STA network interface */
    esp_netif_create_default_wifi_sta();

    /* Initialize WiFi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Register event handler */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));

    /* Set WiFi mode to station */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    /* Configure WiFi settings from menuconfig */
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_OPEN,
        },
    };
    strncpy((char*)wifi_config.sta.ssid, CONFIG_ESP_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, CONFIG_ESP_WIFI_PASSWORD, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    /* Start WiFi */
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi init done, started in STA mode");

    /* Auto-start camera immediately (WiFi may already be connected) */
    camera_ctrl_enable(NULL, NULL);
    camera_server_start();

    ESP_LOGI(TAG, "ESP32 Camera app started");
}