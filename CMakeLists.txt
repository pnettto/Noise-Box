cmake_minimum_required(VERSION 3.22)
project(NOISE_BOX VERSION 0.1.0)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")

add_subdirectory(JUCE)

# Shared header-only kit used by every box: look & feel, user presets, ...
add_library(boxkit INTERFACE)
target_include_directories(boxkit INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Each box is its own plugin. Add new boxes here.
add_subdirectory(boxes/JazzBox)
add_subdirectory(boxes/StonerBox)
