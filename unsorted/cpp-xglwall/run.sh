#!/bin/sh

mkdir -p build
cd build

gcc -c ../xglwall.c -o xglwall.o
[ $? -eq 0 ] || exit 1

ar rcs libxglwall.a xglwall.o

gcc ../example.c -L. -lxglwall $(pkg-config --cflags --libs glew xrender) -o xglwalltest
[ $? -eq 0 ] || exit 1

./xglwalltest
