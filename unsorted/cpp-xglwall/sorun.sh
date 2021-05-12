#!/bin/sh

./sobuild.sh
[ $? -eq 0 ] || exit 1

cd build

gcc ../example.c $(pkg-config --cflags --libs glew) -L. -lxglwall -o test
[ $? -eq 0 ] || exit 1

LD_LIBRARY_PATH=. ./test
