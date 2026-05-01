#include <stdio.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "ws_echo.h"
#include "esp_log.h"

static const char* TAG = "app";

#define WIFI_SSID     "jhome"
#define WIFI_PASS     "123698745"
#define WIFI_CONNECT_TIMEOUT_MS 30000

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "[WiFi] Start connecting...");
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "[WiFi] Disconnected");
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "[WiFi] Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void on_receive(const char* data, int len)
{
    ESP_LOGI(TAG, "[RCV] %s", data);
    // Debug only - do not echo back to avoid echo loop
    // ws_echo_send(data);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32 WS Echo Debug ===");

    // NVS init
    ESP_ERROR_CHECK(nvs_flash_init());

    // TCP/IP init
    ESP_ERROR_CHECK(esp_netif_init());

    // Event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // WiFi init
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handler
    esp_event_handler_instance_t handler;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
        &wifi_event_handler, NULL, &handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
        &wifi_event_handler, NULL, &handler));

    // Configure station
    wifi_config_t wifi_cfg = {
        .sta = {
            .threshold = { .authmode = WIFI_AUTH_WPA2_PSK },
        },
    };
    memcpy(wifi_cfg.sta.ssid, WIFI_SSID, strlen(WIFI_SSID));
    memcpy(wifi_cfg.sta.password, WIFI_PASS, strlen(WIFI_PASS));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "[WiFi] WiFi started, connecting to SSID='%s'", WIFI_SSID);

    // Wait for IP (simple polling)
    int64_t start = esp_timer_get_time() / 1000;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(500));
        int64_t now = esp_timer_get_time() / 1000;
        if (now - start > WIFI_CONNECT_TIMEOUT_MS) {
            ESP_LOGE(TAG, "[WiFi] Timeout waiting for IP!");
            break;
        }
        // Check netif has IP - simplified
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif) {
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                if (ip_info.ip.addr != 0) {
                    ESP_LOGI(TAG, "[WiFi] Connected! IP: " IPSTR, IP2STR(&ip_info.ip));
                    break;
                }
            }
        }
    }

    // Start WS echo
    ESP_LOGI(TAG, "Starting WS echo client...");
    ws_echo_init(on_receive);

    ESP_LOGI(TAG, "=== System running ===");
    ESP_LOGI(TAG, "WiFi SSID='%s' PASS='%s'", WIFI_SSID, WIFI_PASS);
    ESP_LOGI(TAG, "Target server: %s", WS_ECHO_SERVER_URI);
    ESP_LOGI(TAG, "Heartbeat every %d ms", WS_ECHO_HEARTBEAT_MS);
}
