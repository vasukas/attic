#!/bin/sh

mkdir -p build
rm build/*

cp *.c build
cp Makefile build

cd build
make
