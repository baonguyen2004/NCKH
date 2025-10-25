idf_component_register(
    SRCS "main.c"          
    INCLUDE_DIRS "."
    REQUIRES wifi_sta my_mqtt nvs_flash ep_data bt
)
