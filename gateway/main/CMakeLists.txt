idf_component_register(
    SRCS "main.c"          # hoặc "app_main.c" nếu bạn đang dùng tên đó
    INCLUDE_DIRS "."
    REQUIRES wifi_sta my_mqtt nvs_flash ep_data
)
