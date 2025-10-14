# Đăng ký component wifi_sta
idf_component_register(
    SRCS "wifi_sta.c"
    INCLUDE_DIRS "."
    # Nếu IDF của bạn hơi cũ mà kén PRIV_REQUIRES, có thể bỏ dòng dưới
    PRIV_REQUIRES my_mqtt esp_wifi esp_event esp_netif nvs_flash
)
