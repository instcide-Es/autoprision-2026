#!/bin/sh
# Compila autoprision2026. Hace falta: gcc, pkg-config, libgtk-3-dev y libxtst-dev
#   Ubuntu/Debian: sudo apt install build-essential pkg-config libgtk-3-dev libxtst-dev
#   Fedora:        sudo dnf install gcc pkgconf gtk3-devel libXtst-devel
#   Arch:          sudo pacman -S base-devel gtk3 libxtst
set -e
cd "$(dirname "$0")"
gcc -O2 -Wall -Wextra -Wno-unused-parameter -o autoprision2026 autoprision.c \
  $(pkg-config --cflags --libs gtk+-3.0 x11 xtst) -lpthread
echo "Hecho: autoprision2026"
