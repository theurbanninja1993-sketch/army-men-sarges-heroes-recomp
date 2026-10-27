FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates git cmake ninja-build clang python3 patchelf \
    libvulkan-dev glslc libasound2-dev libpulse-dev libx11-dev libxext-dev \
    libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxtst-dev \
    libwayland-dev libxkbcommon-dev libdecor-0-dev libudev-dev libdrm-dev libgbm-dev \
    libegl1-mesa-dev libgl1-mesa-dev && rm -rf /var/lib/apt/lists/*
RUN git clone --depth 1 --branch release-3.4.18 https://github.com/libsdl-org/SDL.git /opt/SDL \
    && test "$(git -C /opt/SDL rev-parse HEAD)" = "829a65d769d935c4852f8159e964312c0957260a" \
    && cmake -S /opt/SDL -B /opt/SDL-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_INSTALL_PREFIX=/opt/sdl3 -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF \
    && cmake --build /opt/SDL-build --parallel 8 \
    && cmake --install /opt/SDL-build
ENV CMAKE_PREFIX_PATH=/opt/sdl3
ENV CC=clang CXX=clang++
