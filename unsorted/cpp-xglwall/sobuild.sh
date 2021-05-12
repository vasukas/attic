#!/bin/sh

mkdir -p build
cd build

gcc -c ../xglwall.c -fPIC
[ $? -eq 0 ] || exit 1

gcc -shared -o libxglwall.so xglwall.o $(pkg-config --cflags --libs x11 xrender glx)
