idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp32-camera
        esp_event
        esp_http_server
        esp_netif
        esp_psram
        esp_wifi
        nvs_flash
        spi_flash
)
