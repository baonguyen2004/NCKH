idf_component_register(
    SRCS "app_mqtt.c"
    INCLUDE_DIRS "."
    REQUIRES ep_data mqtt
    PRIV_REQUIRES esp_wifi esp_event esp_netif nvs_flash ep_data
)