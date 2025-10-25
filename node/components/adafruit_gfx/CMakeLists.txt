idf_component_register(
  SRCS
    "Adafruit_GFX.cpp"
    "glcdfont.c"
    # Nếu thư viện của bạn CÓ file này và KHÔNG kéo thêm phụ thuộc thì có thể thêm vào:
    # "Adafruit_GrayOLED.cpp"
  INCLUDE_DIRS
    "."
  REQUIRES
    arduino-esp32
    adafruit_busio
)
