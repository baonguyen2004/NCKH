idf_component_register(
  SRCS
    "src/GxEPD2_EPD.cpp"
    "src/epd3c/GxEPD2_290c.cpp"   # <-- PHẢI có file này cho panel 3-color Z98c
    "src/epd3c/GxEPD2_213_Z98c.cpp" 
  INCLUDE_DIRS
    "src"
    "src/epd3c"
  REQUIRES
    arduino-esp32
    adafruit_gfx
    adafruit_busio
)
