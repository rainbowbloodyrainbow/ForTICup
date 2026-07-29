#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_camera.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_psram.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

/* User-editable application settings. */
#define APP_NAME                    "H-BALL ESP32-S3 Camera"
#define WIFI_AP_SSID                "H_BALL_CAM"
#define WIFI_AP_PASSWORD            "hballcam"
#define WIFI_AP_CHANNEL             6
#define WIFI_AP_MAX_CLIENTS         2
#define CAMERA_FRAME_SIZE           FRAMESIZE_VGA
#define CAMERA_JPEG_QUALITY         14
#define CAMERA_FB_COUNT             2
#define CAMERA_XCLK_HZ              20000000
#define CONTROL_HTTP_PORT           80
#define STREAM_HTTP_PORT            81
#define HTTP_TASK_STACK_SIZE        8192
#define STREAM_TASK_STACK_SIZE      8192
#define STREAM_TASK_PRIORITY        4
#define HTTP_MAX_OPEN_SOCKETS       4
#define HTTP_RECV_TIMEOUT_SECONDS   5
#define HTTP_SEND_TIMEOUT_SECONDS   5
#define STATS_PERIOD_MS             5000
#define MAX_CONSECUTIVE_FRAME_FAILS 5

/* OV2640 DVP/SCCB wiring. D7 is Y9 and D0 is Y2. */
#define CAM_PIN_PWDN   (-1)
#define CAM_PIN_RESET  (-1)
#define CAM_PIN_XCLK   15
#define CAM_PIN_SIOD   4
#define CAM_PIN_SIOC   5
#define CAM_PIN_D7     16
#define CAM_PIN_D6     17
#define CAM_PIN_D5     18
#define CAM_PIN_D4     12
#define CAM_PIN_D3     10
#define CAM_PIN_D2     8
#define CAM_PIN_D1     9
#define CAM_PIN_D0     11
#define CAM_PIN_VSYNC  6
#define CAM_PIN_HREF   7
#define CAM_PIN_PCLK   13

static const char *TAG_APP = "CAM_APP";
static const char *TAG_WIFI = "CAM_WIFI";
static const char *TAG_HTTP = "CAM_HTTP";
static const char *TAG_STREAM = "CAM_STREAM";
static const char *TAG_STATS = "CAM_STATS";

static httpd_handle_t s_control_server;
static httpd_handle_t s_stream_server;
static SemaphoreHandle_t s_stream_mutex;
static bool s_stream_active;
static uint16_t s_camera_pid;

typedef struct {
    uint64_t frames_sent;
    uint64_t jpeg_bytes_sent;
    uint64_t stream_errors;
    float recent_fps;
    uint32_t recent_average_jpeg_size;
} stream_stats_t;

static stream_stats_t s_stats;
static portMUX_TYPE s_stats_lock = portMUX_INITIALIZER_UNLOCKED;

static const char INDEX_HTML[] =
    "<!doctype html><html lang=\"zh-CN\"><head>"
    "<meta charset=\"utf-8\"><meta name=\"viewport\" "
    "content=\"width=device-width,initial-scale=1\">"
    "<title>" APP_NAME "</title><style>"
    "body{font-family:sans-serif;max-width:900px;margin:2rem auto;padding:0 1rem;"
    "background:#111;color:#eee}img{width:100%;height:auto;background:#222}"
    "a{color:#6cf}code{word-break:break-all}</style></head><body>"
    "<h1>" APP_NAME "</h1>"
    "<img src=\"http://192.168.4.1:81/stream\" alt=\"MJPEG stream\">"
    "<p>视频流：<a href=\"http://192.168.4.1:81/stream\">"
    "<code>http://192.168.4.1:81/stream</code></a></p>"
    "<p>单帧：<a href=\"/capture\"><code>http://192.168.4.1/capture</code></a></p>"
    "<p>状态：<a href=\"/status\"><code>http://192.168.4.1/status</code></a></p>"
    "</body></html>";

static void stats_record_frame(size_t jpeg_size)
{
    portENTER_CRITICAL(&s_stats_lock);
    ++s_stats.frames_sent;
    s_stats.jpeg_bytes_sent += jpeg_size;
    portEXIT_CRITICAL(&s_stats_lock);
}

static void stats_record_error(void)
{
    portENTER_CRITICAL(&s_stats_lock);
    ++s_stats.stream_errors;
    portEXIT_CRITICAL(&s_stats_lock);
}

static stream_stats_t stats_snapshot(void)
{
    stream_stats_t snapshot;
    portENTER_CRITICAL(&s_stats_lock);
    snapshot = s_stats;
    portEXIT_CRITICAL(&s_stats_lock);
    return snapshot;
}

static void stats_task(void *arg)
{
    (void)arg;
    uint64_t previous_frames = 0;
    uint64_t previous_bytes = 0;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(STATS_PERIOD_MS));

        stream_stats_t snapshot = stats_snapshot();
        const uint64_t period_frames = snapshot.frames_sent - previous_frames;
        const uint64_t period_bytes = snapshot.jpeg_bytes_sent - previous_bytes;
        const float fps = (float)period_frames * 1000.0f / (float)STATS_PERIOD_MS;
        const uint32_t average_size = period_frames == 0
                                          ? 0
                                          : (uint32_t)(period_bytes / period_frames);

        portENTER_CRITICAL(&s_stats_lock);
        s_stats.recent_fps = fps;
        s_stats.recent_average_jpeg_size = average_size;
        snapshot.stream_errors = s_stats.stream_errors;
        portEXIT_CRITICAL(&s_stats_lock);

        previous_frames = snapshot.frames_sent;
        previous_bytes = snapshot.jpeg_bytes_sent;

        ESP_LOGI(TAG_STATS,
                 "FPS=%.1f avg_jpeg=%" PRIu32 " bytes total_frames=%" PRIu64
                 " stream_errors=%" PRIu64 " internal_heap=%u psram_free=%u",
                 (double)fps, average_size, snapshot.frames_sent,
                 snapshot.stream_errors,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    }
}

static esp_err_t set_no_cache_headers(httpd_req_t *req)
{
    esp_err_t err = httpd_resp_set_hdr(req, "Cache-Control",
                                       "no-store, no-cache, must-revalidate, max-age=0");
    if (err == ESP_OK) {
        err = httpd_resp_set_hdr(req, "Pragma", "no-cache");
    }
    if (err == ESP_OK) {
        err = httpd_resp_set_hdr(req, "Expires", "0");
    }
    return err;
}

static esp_err_t index_handler(httpd_req_t *req)
{
    esp_err_t err = httpd_resp_set_type(req, "text/html; charset=utf-8");
    if (err != ESP_OK) {
        return err;
    }
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t capture_handler(httpd_req_t *req)
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == NULL) {
        ESP_LOGE(TAG_HTTP, "Single-frame capture failed");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Camera capture failed");
    }

    esp_err_t err;
    if (fb->format != PIXFORMAT_JPEG) {
        ESP_LOGE(TAG_HTTP, "Rejected non-JPEG frame (format=%d)", fb->format);
        esp_camera_fb_return(fb);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Camera returned a non-JPEG frame");
    }

    err = httpd_resp_set_type(req, "image/jpeg");
    if (err == ESP_OK) {
        err = set_no_cache_headers(req);
    }
    if (err == ESP_OK) {
        err = httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    }
    if (err == ESP_OK) {
        err = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    }

    esp_camera_fb_return(fb);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_HTTP, "Single-frame response failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    stream_stats_t snapshot = stats_snapshot();
    char json[320];
    const int length = snprintf(
        json, sizeof(json),
        "{\"frames_sent\":%" PRIu64 ",\"stream_errors\":%" PRIu64
        ",\"recent_fps\":%.2f,\"recent_average_jpeg_size\":%" PRIu32
        ",\"internal_heap_free\":%u,\"psram_free\":%u,\"camera_pid\":%u}",
        snapshot.frames_sent, snapshot.stream_errors,
        (double)snapshot.recent_fps, snapshot.recent_average_jpeg_size,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        (unsigned)s_camera_pid);

    if (length < 0 || (size_t)length >= sizeof(json)) {
        ESP_LOGE(TAG_HTTP, "Status JSON buffer is too small");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Status encoding failed");
    }

    esp_err_t err = httpd_resp_set_type(req, "application/json");
    if (err == ESP_OK) {
        err = set_no_cache_headers(req);
    }
    if (err == ESP_OK) {
        err = httpd_resp_send(req, json, length);
    }
    return err;
}

static bool claim_stream_client(void)
{
    bool claimed = false;
    if (xSemaphoreTake(s_stream_mutex, portMAX_DELAY) == pdTRUE) {
        if (!s_stream_active) {
            s_stream_active = true;
            claimed = true;
        }
        xSemaphoreGive(s_stream_mutex);
    }
    return claimed;
}

static void release_stream_client(void)
{
    if (xSemaphoreTake(s_stream_mutex, portMAX_DELAY) == pdTRUE) {
        s_stream_active = false;
        xSemaphoreGive(s_stream_mutex);
    }
}

static esp_err_t stream_session(httpd_req_t *req)
{
    static const char *STREAM_TYPE =
        "multipart/x-mixed-replace;boundary=frame";
    static const char *BOUNDARY = "\r\n--frame\r\n";
    static const char *PART_HEADER_FORMAT =
        "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

    ESP_LOGI(TAG_STREAM, "Stream client connected");
    esp_err_t err = httpd_resp_set_type(req, STREAM_TYPE);
    if (err == ESP_OK) {
        err = set_no_cache_headers(req);
    }
    if (err == ESP_OK) {
        err = httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    }

    unsigned consecutive_failures = 0;
    while (err == ESP_OK) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb == NULL) {
            ++consecutive_failures;
            stats_record_error();
            ESP_LOGE(TAG_STREAM, "Frame capture failed (%u/%u)",
                     consecutive_failures, MAX_CONSECUTIVE_FRAME_FAILS);
            if (consecutive_failures >= MAX_CONSECUTIVE_FRAME_FAILS) {
                err = ESP_FAIL;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (fb->format != PIXFORMAT_JPEG) {
            ++consecutive_failures;
            stats_record_error();
            ESP_LOGE(TAG_STREAM, "Rejected non-JPEG frame (format=%d, %u/%u)",
                     fb->format, consecutive_failures,
                     MAX_CONSECUTIVE_FRAME_FAILS);
            esp_camera_fb_return(fb);
            if (consecutive_failures >= MAX_CONSECUTIVE_FRAME_FAILS) {
                err = ESP_ERR_INVALID_RESPONSE;
            }
            continue;
        }

        char part_header[96];
        const int header_length = snprintf(part_header, sizeof(part_header),
                                           PART_HEADER_FORMAT,
                                           (unsigned)fb->len);
        if (header_length < 0 || (size_t)header_length >= sizeof(part_header)) {
            ESP_LOGE(TAG_STREAM, "MJPEG part header was truncated");
            esp_camera_fb_return(fb);
            stats_record_error();
            err = ESP_ERR_INVALID_SIZE;
            break;
        }

        err = httpd_resp_send_chunk(req, BOUNDARY, HTTPD_RESP_USE_STRLEN);
        if (err == ESP_OK) {
            err = httpd_resp_send_chunk(req, part_header, header_length);
        }
        if (err == ESP_OK) {
            err = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        }
        if (err == ESP_OK) {
            err = httpd_resp_send_chunk(req, "\r\n", 2);
        }

        const size_t jpeg_size = fb->len;
        esp_camera_fb_return(fb);

        if (err == ESP_OK) {
            consecutive_failures = 0;
            stats_record_frame(jpeg_size);
        } else {
            stats_record_error();
            ESP_LOGW(TAG_STREAM, "Stream send ended: %s", esp_err_to_name(err));
        }
    }

    return err;
}

static void stream_worker_task(void *arg)
{
    httpd_req_t *async_req = arg;
    const esp_err_t stream_err = stream_session(async_req);

    release_stream_client();
    ESP_LOGI(TAG_STREAM, "Stream client disconnected; slot released (%s)",
             esp_err_to_name(stream_err));

    const esp_err_t complete_err =
        httpd_req_async_handler_complete(async_req);
    if (complete_err != ESP_OK) {
        ESP_LOGE(TAG_STREAM, "Async request cleanup failed: %s",
                 esp_err_to_name(complete_err));
    }
    vTaskDelete(NULL);
}

static esp_err_t stream_handler(httpd_req_t *req)
{
    if (!claim_stream_client()) {
        ESP_LOGW(TAG_STREAM, "Rejected second simultaneous stream client");
        esp_err_t err = httpd_resp_set_status(req, "503 Service Unavailable");
        if (err == ESP_OK) {
            err = httpd_resp_set_type(req, "text/plain");
        }
        if (err == ESP_OK) {
            err = httpd_resp_set_hdr(req, "Retry-After", "2");
        }
        if (err == ESP_OK) {
            err = httpd_resp_send(req, "Only one stream client is allowed",
                                  HTTPD_RESP_USE_STRLEN);
        }
        return err;
    }

    httpd_req_t *async_req = NULL;
    esp_err_t err = httpd_req_async_handler_begin(req, &async_req);
    if (err != ESP_OK) {
        release_stream_client();
        ESP_LOGE(TAG_STREAM, "Unable to create async stream request: %s",
                 esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Unable to start stream worker");
    }

    if (xTaskCreate(stream_worker_task, "mjpeg_stream",
                    STREAM_TASK_STACK_SIZE, async_req,
                    STREAM_TASK_PRIORITY, NULL) != pdPASS) {
        release_stream_client();
        ESP_LOGE(TAG_STREAM, "Unable to create stream worker task");
        const esp_err_t complete_err =
            httpd_req_async_handler_complete(async_req);
        if (complete_err != ESP_OK) {
            ESP_LOGE(TAG_STREAM, "Async request rollback failed: %s",
                     esp_err_to_name(complete_err));
        }
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Unable to start stream worker");
    }

    return ESP_OK;
}

static esp_err_t register_uri(httpd_handle_t server, const char *uri,
                              httpd_method_t method, esp_err_t (*handler)(httpd_req_t *))
{
    const httpd_uri_t config = {
        .uri = uri,
        .method = method,
        .handler = handler,
        .user_ctx = NULL,
    };
    const esp_err_t err = httpd_register_uri_handler(server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_HTTP, "Failed to register %s: %s", uri, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t start_http_servers(void)
{
    httpd_config_t control_config = HTTPD_DEFAULT_CONFIG();
    control_config.server_port = CONTROL_HTTP_PORT;
    control_config.ctrl_port = 32768;
    control_config.stack_size = HTTP_TASK_STACK_SIZE;
    control_config.max_open_sockets = HTTP_MAX_OPEN_SOCKETS;
    control_config.max_uri_handlers = 4;
    control_config.lru_purge_enable = true;
    control_config.recv_wait_timeout = HTTP_RECV_TIMEOUT_SECONDS;
    control_config.send_wait_timeout = HTTP_SEND_TIMEOUT_SECONDS;

    esp_err_t err = httpd_start(&s_control_server, &control_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_HTTP, "Control server start failed: %s", esp_err_to_name(err));
        return err;
    }

    if ((err = register_uri(s_control_server, "/", HTTP_GET, index_handler)) != ESP_OK ||
        (err = register_uri(s_control_server, "/capture", HTTP_GET, capture_handler)) != ESP_OK ||
        (err = register_uri(s_control_server, "/status", HTTP_GET, status_handler)) != ESP_OK) {
        httpd_stop(s_control_server);
        s_control_server = NULL;
        return err;
    }

    httpd_config_t stream_config = HTTPD_DEFAULT_CONFIG();
    stream_config.server_port = STREAM_HTTP_PORT;
    stream_config.ctrl_port = 32769;
    stream_config.stack_size = HTTP_TASK_STACK_SIZE;
    stream_config.max_open_sockets = HTTP_MAX_OPEN_SOCKETS;
    stream_config.max_uri_handlers = 1;
    stream_config.lru_purge_enable = true;
    stream_config.recv_wait_timeout = HTTP_RECV_TIMEOUT_SECONDS;
    stream_config.send_wait_timeout = HTTP_SEND_TIMEOUT_SECONDS;

    err = httpd_start(&s_stream_server, &stream_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_HTTP, "Stream server start failed: %s", esp_err_to_name(err));
        httpd_stop(s_control_server);
        s_control_server = NULL;
        return err;
    }

    err = register_uri(s_stream_server, "/stream", HTTP_GET, stream_handler);
    if (err != ESP_OK) {
        httpd_stop(s_stream_server);
        httpd_stop(s_control_server);
        s_stream_server = NULL;
        s_control_server = NULL;
        return err;
    }

    ESP_LOGI(TAG_HTTP, "Control: http://192.168.4.1/");
    ESP_LOGI(TAG_HTTP, "Stream:  http://192.168.4.1:81/stream");
    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;

    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *event = event_data;
        ESP_LOGI(TAG_WIFI, "Station " MACSTR " joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t *event = event_data;
        ESP_LOGI(TAG_WIFI, "Station " MACSTR " left, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

static esp_err_t init_softap(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK) {
        return err;
    }
    if (esp_netif_create_default_wifi_ap() == NULL) {
        return ESP_ERR_NO_MEM;
    }

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_config);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                     wifi_event_handler, NULL);
    if (err != ESP_OK) {
        return err;
    }

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .password = WIFI_AP_PASSWORD,
            .ssid_len = sizeof(WIFI_AP_SSID) - 1,
            .channel = WIFI_AP_CHANNEL,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden = 0,
            .max_connection = WIFI_AP_MAX_CLIENTS,
            .beacon_interval = 100,
        },
    };

    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err == ESP_OK) {
        err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    }
    if (err == ESP_OK) {
        err = esp_wifi_start();
    }
    if (err == ESP_OK) {
        err = esp_wifi_set_ps(WIFI_PS_NONE);
    }
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG_WIFI, "SoftAP started: SSID=%s channel=%d IP=192.168.4.1",
             WIFI_AP_SSID, WIFI_AP_CHANNEL);
    return ESP_OK;
}

static esp_err_t init_camera(void)
{
    const camera_config_t camera_config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,
        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,
        .xclk_freq_hz = CAMERA_XCLK_HZ,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = CAMERA_FRAME_SIZE,
        .jpeg_quality = CAMERA_JPEG_QUALITY,
        .fb_count = CAMERA_FB_COUNT,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_LATEST,
    };

    const esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_APP, "Camera initialization failed: %s (0x%x)",
                 esp_err_to_name(err), (unsigned)err);
        return err;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor == NULL) {
        ESP_LOGE(TAG_APP, "Camera initialized but sensor descriptor is unavailable");
        esp_camera_deinit();
        return ESP_ERR_NOT_FOUND;
    }

    s_camera_pid = sensor->id.PID;
    ESP_LOGI(TAG_APP, "Sensor ID: PID=0x%02x VER=0x%02x MIDH=0x%02x MIDL=0x%02x",
             sensor->id.PID, sensor->id.VER, sensor->id.MIDH, sensor->id.MIDL);
    return ESP_OK;
}

static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG_APP, "Erasing incompatible/full NVS partition");
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    return err;
}

void app_main(void)
{
    ESP_LOGI(TAG_APP, "Starting %s", APP_NAME);

    uint32_t flash_size = 0;
    esp_err_t err = esp_flash_get_size(NULL, &flash_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_APP, "Unable to query Flash capacity: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG_APP, "Flash capacity: %" PRIu32 " bytes (%" PRIu32 " MB)",
             flash_size, flash_size / (1024U * 1024U));

    if (!esp_psram_is_initialized()) {
        ESP_LOGE(TAG_APP, "PSRAM was not detected/initialized; HTTP video will not start");
        return;
    }
    ESP_LOGI(TAG_APP, "PSRAM total: %u bytes, free: %u bytes",
             (unsigned)esp_psram_get_size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG_APP, "Internal heap free: %u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    err = init_nvs();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_APP, "NVS initialization failed: %s", esp_err_to_name(err));
        return;
    }

    err = init_camera();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_APP, "HTTP video startup aborted because camera is unavailable");
        return;
    }

    s_stream_mutex = xSemaphoreCreateMutex();
    if (s_stream_mutex == NULL) {
        ESP_LOGE(TAG_APP, "Unable to create stream-client mutex");
        esp_camera_deinit();
        return;
    }

    err = init_softap();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_APP, "SoftAP initialization failed: %s", esp_err_to_name(err));
        esp_camera_deinit();
        return;
    }

    err = start_http_servers();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_APP, "HTTP server startup failed: %s", esp_err_to_name(err));
        return;
    }

    if (xTaskCreate(stats_task, "camera_stats", 3072, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG_APP, "Unable to start statistics task");
    }
}
