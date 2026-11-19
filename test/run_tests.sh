#!/usr/bin/env bash

BUILD_TYPE="Debug"

cmake -B build -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DNTF_TESTS=1
make -C build -j$(nproc)
ctest --test-dir build/test -V --progress
