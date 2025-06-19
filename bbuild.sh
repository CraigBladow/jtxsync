#!/bin/sh

# Detect OS
ios_name="$(uname -s)"

if [ "$ios_name" = "Darwin" ]; then
    echo "Building jtxsync for macOS."
    mkdir -p ./build
    gcc -arch arm64 -arch x86_64 ./source/main.c  -o ./build/jtxsync

elif [ "$ios_name" = "Linux" ]; then
    echo "Building jtxsync for Linux."
    mkdir -p ./build
    gcc -static -s ./source/main.c -lm -o ./build/jtxsync

else
    echo "Unknown operating system: $ios_name"
     exit 1
fi




