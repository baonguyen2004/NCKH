cmake_minimum_required(VERSION 3.5)

# Ép CMake quét đúng thư mục components (an toàn cho mọi biến thể dự án)
set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/components")

# ESP-IDF project glue
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(Gateway)