cmake_minimum_required(VERSION 3.16)
project(circuit_player LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(raylib 4.0 QUIET)
if (NOT raylib_FOUND)
    message(FATAL_ERROR
        "raylib was not found. Install raylib and make its CMake package available.")
endif()

add_executable(circuit-player
    src/main.cpp
    src/CsvLoader.cpp
    src/Playback.cpp
    src/Renderer.cpp
)

target_include_directories(circuit-player PRIVATE src)
target_link_libraries(circuit-player PRIVATE raylib)

if (APPLE)
    target_link_libraries(circuit-player PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
