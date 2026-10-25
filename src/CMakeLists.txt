idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../include"
    REQUIRES
        driver
        esp_event
        esp_http_server
        esp_netif
        esp_wifi
        mdns
        nvs_flash
)
