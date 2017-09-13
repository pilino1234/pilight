#!/bin/bash -e

if [[ ${PLATFORM} == "Unix" ]]; then
    mkdir -p build
    cd build || exit 1

    cmake -DPILIGHT_UNITTEST=1 \
        -DCOVERALLS=ON \
        -DCMAKE_BUILD_TYPE=Debug ..
    make -j2

    sudo LD_PRELOAD=libgpio.so ./pilight-unittest && exit 0
fi
