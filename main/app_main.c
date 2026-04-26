#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_netif_types.h"

#include "camera_ctrl.h"
#include "camera_server.h"
#include "ws_control.h"
#include "servo.h"

static const char* TAG = "app_main";

/* Static IP configuration */
#define ESP32_IP      "10.0.0.110"
#define ESP32_GW      "10.0.0.2"
#define ESP32_NETMASK "255.255.255.0"

/* Store netif pointer globally for static IP config */
static esp_netif_t* s_netif = NULL;

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
        }
    }
}

/* Control callback */
static void ws_control_callback(ws_action_t action, void* user_data)
{
    char response[128];

    switch (action) {
        case WS_ACTION_ON:
            if (!camera_ctrl_is_on()) {
                camera_err_t err = camera_ctrl_enable(NULL, NULL);
                if (err == CAMERA_OK) {
                    camera_server_start();
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

    /* Initialize TCP/IP stack first */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Create default event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Create default WiFi STA network interface */
    s_netif = esp_netif_create_default_wifi_sta();

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

    /* Configure WiFi settings from sdkconfig */
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA_PSK,
            .pmf_cfg = { .capable = true, .required = false },
        },
    };
    strncpy((char*)wifi_config.sta.ssid, CONFIG_ESP_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, CONFIG_ESP_WIFI_PASSWORD, sizeof(wifi_config.sta.password));
    ESP_LOGI(TAG, "WiFi config: SSID=%s", CONFIG_ESP_WIFI_SSID);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    /* Start WiFi */
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi init done, started in STA mode");

    /* Initialize servo */
    servo_init();

    /* Initialize camera control */
    camera_ctrl_init();

    /* Initialize HTTP control server */
    ws_control_init(ws_control_callback, NULL);

    /* Wait for WiFi connection - check connection state periodically */
    ESP_LOGI(TAG, "Waiting for WiFi connection...");

    bool connected = false;
    int connect_timeout = 15000;
    int elapsed = 0;

    while (!connected && elapsed < connect_timeout) {
        vTaskDelay(pdMS_TO_TICKS(100));
        elapsed += 100;

        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            connected = true;
            ESP_LOGI(TAG, "WiFi connected to AP: %s", ap_info.ssid);
            break;
        }
    }

    if (!connected) {
        ESP_LOGW(TAG, "WiFi connection timeout, proceeding anyway");
    }

    /* Configure static IP after WiFi connects */
    if (s_netif) {
        /* Stop DHCP client before setting static IP */
        esp_err_t err = esp_netif_dhcpc_stop(s_netif);
        if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
            ESP_LOGW(TAG, "DHCP stop failed: %s", esp_err_to_name(err));
        }

        esp_netif_ip_info_t ip_info = {0};
        ip_info.ip.addr = esp_ip4addr_aton(ESP32_IP);
        ip_info.netmask.addr = esp_ip4addr_aton(ESP32_NETMASK);
        ip_info.gw.addr = esp_ip4addr_aton(ESP32_GW);
        ESP_ERROR_CHECK(esp_netif_set_ip_info(s_netif, &ip_info));

        /* Set DNS server */
        esp_netif_dns_info_t dns_info = {0};
        dns_info.ip.type = ESP_IPADDR_TYPE_V4;
        dns_info.ip.u_addr.ip4.addr = esp_ip4addr_aton(ESP32_GW);
        ESP_ERROR_CHECK(esp_netif_set_dns_info(s_netif, ESP_NETIF_DNS_MAIN, &dns_info));
        ESP_LOGI(TAG, "Static IP/GW/DNS set: " IPSTR " gw " IPSTR " dns " IPSTR,
                 IP2STR(&ip_info.ip), IP2STR(&ip_info.gw), IP2STR(&dns_info.ip.u_addr.ip4));
    } else {
        ESP_LOGE(TAG, "Failed to get netif handle");
    }

    /* Additional stabilization delay */
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* Start camera and stream server */
    ESP_LOGI(TAG, "Starting camera and stream server...");
    if (camera_ctrl_enable(NULL, NULL) == CAMERA_OK) {
        camera_server_start();
    }

    ESP_LOGI(TAG, "ESP32 Camera app started");
}
