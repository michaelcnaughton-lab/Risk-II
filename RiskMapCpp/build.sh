#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if ! command -v sdl2-config >/dev/null 2>&1; then
    echo 'SDL2 development tools are missing. On macOS: brew install sdl2' >&2
    echo 'On Ubuntu/Debian: sudo apt install g++ libsdl2-dev' >&2
    exit 1
fi
read -r -a sdl_cflags <<< "$(sdl2-config --cflags)"
read -r -a sdl_libs <<< "$(sdl2-config --libs)"
"${CXX:-g++}" -std=c++17 -O2 -Wall -Wextra -Wpedantic "${sdl_cflags[@]}" main.cpp -o risk-map "${sdl_libs[@]}"
echo 'Built risk-map. Run: ./risk-map'
