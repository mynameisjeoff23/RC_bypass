#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "webapp_html.h"
#include "webapp_js.h"

#define PIN_DRIVE_FWD    3
#define PIN_DRIVE_REV    4
#define PIN_STEER_LEFT   8
#define PIN_STEER_RIGHT  9

#define LEDC_TIMER       LEDC_TIMER_0
#define LEDC_MODE        LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL_FWD LEDC_CHANNEL_0
#define LEDC_CHANNEL_REV LEDC_CHANNEL_1
#define LEDC_DUTY_RES    LEDC_TIMER_10_BIT
#define LEDC_FREQUENCY   5000
#define DRIVE_START_BOOST_MS 5
#define MIN_DRIVE_SPEED_PERCENT 50

#define AP_SSID          "RC-Car"
#define AP_PASSWORD      "rc-car-control"

static const char *TAG = "RC_CONTROL";

typedef enum { STEER_CENTER = 0, STEER_LEFT, STEER_RIGHT } steering_state_t;
volatile int target_speed_percent = 0;
volatile steering_state_t target_steering = STEER_CENTER;

static httpd_handle_t http_server;

static void stop_outputs(void)
{
    target_speed_percent = 0;
    target_steering = STEER_CENTER;
}

static void set_drive_speed(int speed)
{
    if (speed > 100) speed = 100;
    if (speed < -100) speed = -100;
    if (speed > 0 && speed < MIN_DRIVE_SPEED_PERCENT) {
        speed = MIN_DRIVE_SPEED_PERCENT;
    } else if (speed < 0 && speed > -MIN_DRIVE_SPEED_PERCENT) {
        speed = -MIN_DRIVE_SPEED_PERCENT;
    }

    uint32_t duty = (uint32_t)(abs(speed) * 1023) / 100;
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_FWD, speed < 0 ? duty : 0);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_FWD);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_REV, speed > 0 ? duty : 0);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_REV);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        stop_outputs();
    }
}

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)webapp_index_html,
                           webapp_index_html_length);
}

static esp_err_t app_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    return httpd_resp_send(req, (const char *)webapp_app_js,
                           webapp_app_js_length);
}

static const char *json_value(const char *body, const char *key)
{
    char quoted_key[20];
    int written = snprintf(quoted_key, sizeof(quoted_key), "\"%s\"", key);
    if (written < 0 || written >= (int)sizeof(quoted_key)) {
        return NULL;
    }

    const char *key_start = strstr(body, quoted_key);
    if (key_start == NULL) {
        return NULL;
    }
    const char *colon = strchr(key_start + written, ':');
    return colon == NULL ? NULL : colon + 1;
}

static bool parse_control_body(const char *body, int *throttle,
                               bool *left, bool *right)
{
    char *end;
    const char *value = json_value(body, "throttle");
    if (value == NULL) {
        return false;
    }
    long parsed_throttle = strtol(value, &end, 10);
    if (end == value || parsed_throttle < -100 || parsed_throttle > 100) {
        return false;
    }
    *throttle = (int)parsed_throttle;

    const char *left_value = json_value(body, "left");
    const char *right_value = json_value(body, "right");
    if (left_value == NULL || right_value == NULL) {
        return false;
    }
    if (strncmp(left_value, "true", 4) == 0) {
        *left = true;
    } else if (strncmp(left_value, "false", 5) == 0) {
        *left = false;
    } else {
        return false;
    }
    if (strncmp(right_value, "true", 4) == 0) {
        *right = true;
    } else if (strncmp(right_value, "false", 5) == 0) {
        *right = false;
    } else {
        return false;
    }
    return !(*left && *right);
}

static esp_err_t control_handler(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > 256) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request body");
        return ESP_FAIL;
    }

    char body[257];
    int received = 0;
    while (received < req->content_len) {
        int result = httpd_req_recv(req, body + received, req->content_len - received);
        if (result <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Incomplete request body");
            return ESP_FAIL;
        }
        received += result;
    }
    body[received] = '\0';

    int throttle;
    bool left;
    bool right;
    if (!parse_control_body(body, &throttle, &left, &right)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid control values");
        return ESP_FAIL;
    }

    target_speed_percent = throttle;
    target_steering = left ? STEER_LEFT :
                      right ? STEER_RIGHT : STEER_CENTER;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static void start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 4;
    config.max_uri_handlers = 8;
    ESP_ERROR_CHECK(httpd_start(&http_server, &config));

    static const httpd_uri_t index_uri = {
        .uri = "/", .method = HTTP_GET, .handler = index_handler
    };
    static const httpd_uri_t js_uri = {
        .uri = "/app.js", .method = HTTP_GET, .handler = app_js_handler
    };
    static const httpd_uri_t control_uri = {
        .uri = "/api/control", .method = HTTP_POST, .handler = control_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(http_server, &index_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(http_server, &js_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(http_server, &control_uri));
}

static void start_network(void)
{
    esp_err_t nvs_result = nvs_flash_init();
    if (nvs_result == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_result);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               wifi_event_handler, NULL));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = sizeof(AP_SSID) - 1,
            .channel = 1,
            .password = AP_PASSWORD,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden = 0
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set("car"));
    ESP_ERROR_CHECK(mdns_instance_name_set("RC Car Controller"));
    ESP_ERROR_CHECK(mdns_service_add("RC Car HTTP", "_http", "_tcp",
                                     80, NULL, 0));
    start_http_server();
    ESP_LOGI(TAG, "Connect to %s and open http://car.local", AP_SSID);
}

void rc_car_init(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_STEER_LEFT) | (1ULL << PIN_STEER_RIGHT),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(PIN_STEER_LEFT, 0);
    gpio_set_level(PIN_STEER_RIGHT, 0);

    ledc_timer_config_t timer = {
        .speed_mode = LEDC_MODE, .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES, .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    ledc_channel_config_t forward = {
        .speed_mode = LEDC_MODE, .channel = LEDC_CHANNEL_FWD,
        .timer_sel = LEDC_TIMER, .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = PIN_DRIVE_FWD, .duty = 0, .hpoint = 0
    };
    ledc_channel_config_t reverse = forward;
    reverse.channel = LEDC_CHANNEL_REV;
    reverse.gpio_num = PIN_DRIVE_REV;
    ESP_ERROR_CHECK(ledc_channel_config(&forward));
    ESP_ERROR_CHECK(ledc_channel_config(&reverse));
}

void rc_control_task(void *pvParameters)
{
    int previous_speed = 0;

    while (true) {
        switch (target_steering) {
        case STEER_LEFT:
            gpio_set_level(PIN_STEER_RIGHT, 0);
            gpio_set_level(PIN_STEER_LEFT, 1);
            break;
        case STEER_RIGHT:
            gpio_set_level(PIN_STEER_LEFT, 0);
            gpio_set_level(PIN_STEER_RIGHT, 1);
            break;
        default:
            gpio_set_level(PIN_STEER_LEFT, 0);
            gpio_set_level(PIN_STEER_RIGHT, 0);
            break;
        }

        int speed = target_speed_percent;
        if (speed > 100) speed = 100;
        if (speed < -100) speed = -100;

        if (speed != 0 && previous_speed == 0) {
            set_drive_speed(speed > 0 ? 100 : -100);
            esp_rom_delay_us(DRIVE_START_BOOST_MS * 1000);
            speed = target_speed_percent;
            if (speed > 100) speed = 100;
            if (speed < -100) speed = -100;
        }
        set_drive_speed(speed);
        previous_speed = speed;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    rc_car_init();
    xTaskCreate(rc_control_task, "rc_control_task", 2048, NULL, 5, NULL);
    start_network();
}
