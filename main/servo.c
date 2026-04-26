#include "servo.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "hal/ledc_types.h"
#include "esp_log.h"

static const char* TAG = "servo";

#define SERVO_GPIO            GPIO_NUM_13
#define SERVO_LEDC_CHANNEL    LEDC_CHANNEL_0
#define SERVO_LEDC_TIMER      LEDC_TIMER_0
#define SERVO_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define SERVO_FREQUENCY_HZ    50
#define SERVO_DUTTY_RESOLUTION LEDC_TIMER_13_BIT  // 13-bit = 8191 max

// Use 50Hz exactly: APB=80MHz, div=80, period=20000
#define SERVO_LEDC_DIVIDER    80
#define SERVO_LEDC_PERIOD    20000  // 20ms in us

static int s_current_angle = 90;

static uint32_t angle_to_duty(int angle)
{
    // 0° = 5% duty = 1000us
    // 180° = 20% duty = 4000us
    // Using period in us: 1000-4000us mapped to 0-180°
    // angle * 3000 / 180 maps 0-180 to 0-3000
    uint32_t duty_us = 1000 + (angle * 3000 / 180);
    uint32_t duty = (duty_us * 65535) / SERVO_LEDC_PERIOD;
    return duty;
}

void servo_init(void)
{
    // LEDC timer config
    ledc_timer_config_t timer = {
        .speed_mode = SERVO_LEDC_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = SERVO_LEDC_TIMER,
        .freq_hz = SERVO_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    // LEDC channel config
    ledc_channel_config_t channel = {
        .speed_mode = SERVO_LEDC_MODE,
        .channel = SERVO_LEDC_CHANNEL,
        .timer_sel = SERVO_LEDC_TIMER,  // was .timer
        .gpio_num = SERVO_GPIO,
        .duty = angle_to_duty(90),
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));

    s_current_angle = 90;
    ESP_LOGI(TAG, "Servo initialized at 90°");
}

void servo_set_angle(int angle)
{
    if (angle < SERVO_MIN_ANGLE || angle > SERVO_MAX_ANGLE) {
        ESP_LOGW(TAG, "Angle %d out of range [%d, %d]", angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
        return;
    }
    s_current_angle = angle;
    uint32_t duty = angle_to_duty(angle);
    ESP_ERROR_CHECK(ledc_set_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL));
    ESP_LOGI(TAG, "Set angle to %d° (duty=%u)", angle, duty);
}

int servo_get_angle(void)
{
    return s_current_angle;
}

void servo_deinit(void)
{
    ledc_stop(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL, 0);
    ESP_LOGI(TAG, "Servo deinitialized");
}