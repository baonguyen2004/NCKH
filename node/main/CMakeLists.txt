idf_component_register(
    SRCS
        "main.cpp"
    INCLUDE_DIRS
        "."
    REQUIRES
        arduino-esp32 gxepd2  price_tag_epd   nvs_flash example_init
)
