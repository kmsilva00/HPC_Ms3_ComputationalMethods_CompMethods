#!/usr/bin/env bash
set -e

g++ -std=c++17 \
    src/main.cpp \
    src/CsvLoader.cpp \
    src/Playback.cpp \
    src/Renderer.cpp \
    -o circuit-player \
    -lraylib \
    -lGL -lm -lpthread -ldl -lrt -lX11

./circuit-player "$@"