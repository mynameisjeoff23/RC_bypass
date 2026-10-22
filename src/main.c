#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

static const char *TAG = "RC_CONTROL";

// --- Hardware Pin Definitions (Adjust for your ESP32-C3 wiring) ---
#define PIN_DRIVE_FWD    6  // Connects to YX5080AM Pin 2 or 3
#define PIN_DRIVE_REV    7  // Connects to YX5080AM Pin 3 or 2
#define PIN_STEER_LEFT   8  // Connects to MX08H INA
#define PIN_STEER_RIGHT  9  // Connects to MX08H INB

// --- PWM Configuration (LEDC) for Speed Control ---
#define LEDC_TIMER       LEDC_TIMER_0
#define LEDC_MODE        LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL_FWD LEDC_CHANNEL_0
#define LEDC_CHANNEL_REV LEDC_CHANNEL_1
#define LEDC_DUTY_RES    LEDC_TIMER_10_BIT // 0-1023 range
#define LEDC_FREQUENCY   5000              // 5 kHz PWM

// --- Shared State for WebApp Interface ---
typedef enum { STEER_CENTER = 0, STEER_LEFT, STEER_RIGHT } steering_state_t;

// Volatile variables allow a future Wi-Fi HTTP handler to update targets safely
volatile int target_speed_percent = 0;       // -100 (Max Rev) to 100 (Max Fwd)
volatile steering_state_t target_steering = STEER_CENTER;

// --- Initialization ---
void rc_car_init(void) {
    ESP_LOGI(TAG, "Initializing Motor Control Pins...");

    // 1. Configure Steering GPIOs (MX08H)
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_STEER_LEFT) | (1ULL << PIN_STEER_RIGHT),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(PIN_STEER_LEFT, 0);
    gpio_set_level(PIN_STEER_RIGHT, 0);

    // 2. Configure Drive PWM (YX5080AM)
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_ch_fwd = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_FWD,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_DRIVE_FWD,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_ch_fwd);

    ledc_channel_config_t ledc_ch_rev = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_REV,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_DRIVE_REV,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_ch_rev);
}

// --- Control Task ---
void rc_control_task(void *pvParameters) {
    while (1) {
        // 1. Apply Steering State (MX08H Logic)
        switch (target_steering) {
            case STEER_LEFT:
                gpio_set_level(PIN_STEER_RIGHT, 0);
                gpio_set_level(PIN_STEER_LEFT, 1);
                break;
            case STEER_RIGHT:
                gpio_set_level(PIN_STEER_LEFT, 0);
                gpio_set_level(PIN_STEER_RIGHT, 1);
                break;
            case STEER_CENTER:
            default:
                gpio_set_level(PIN_STEER_LEFT, 0);
                gpio_set_level(PIN_STEER_RIGHT, 0);
                break;
        }

        // 2. Apply Speed State (YX5080AM Logic)
        // Clamp speed between -100 and 100
        int speed = target_speed_percent;
        if (speed > 100) speed = 100;
        if (speed < -100) speed = -100;

        uint32_t duty = (abs(speed) * 1023) / 100; 

        if (speed > 0) {
            // Forward
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_REV, 0);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_REV);
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_FWD, duty);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_FWD);
        } else if (speed < 0) {
            // Reverse
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_FWD, 0);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_FWD);
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_REV, duty);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_REV);
        } else {
            // Coast/Stop
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_FWD, 0);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_FWD);
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_REV, 0);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_REV);
        }

        // Run the control update loop at ~20Hz (50ms interval)
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// --- Application Entry Point ---
void app_main(void) {
    rc_car_init();

    // Spawn the motor control background task
    xTaskCreate(rc_control_task, "rc_control_task", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "Executing slow diagnostic sequence...");

    // Example sequence demonstrating how the webapp will interface with the vehicle
    target_speed_percent = 50; 
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    target_speed_percent = 0;
    target_steering = STEER_LEFT;
    vTaskDelay(pdMS_TO_TICKS(1000));

    target_steering = STEER_RIGHT;
    target_speed_percent = -50;
    vTaskDelay(pdMS_TO_TICKS(1000));

    target_steering = STEER_CENTER;
    target_speed_percent = 0;
}