FROM debian:bookworm AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential git ca-certificates curl pkg-config binutils unzip zip python3 \
      libsdl2-dev libsdl2-ttf-dev libsdl2-mixer-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# FFmpeg: static build with the codecs we need (no GPL/nonfree)
RUN git clone --depth 1 --branch n6.1.1 https://github.com/FFmpeg/FFmpeg.git ffmpeg
RUN cd ffmpeg && ./configure --prefix=/build/ffout \
      --disable-everything --disable-programs --disable-doc --disable-debug --disable-autodetect \
      --disable-network --disable-avdevice --disable-avfilter --disable-zlib --disable-iconv \
      --disable-shared --enable-static --enable-pic --enable-pthreads \
      --enable-protocol=file \
      --enable-demuxer=mov,matroska,avi,webm,flac \
      --enable-parser=h264,hevc,vp9,av1,aac,aac_latm,mpegaudio,ac3,vorbis,opus \
      --enable-decoder=h264,hevc,vp9,av1,aac,mp3,mp3float,ac3,eac3,vorbis,opus \
    && make -j"$(nproc)" && make install

RUN curl -fsSL -o dr_mp3.h https://raw.githubusercontent.com/mackron/dr_libs/master/dr_mp3.h

COPY src ./src
RUN gcc -O2 -std=gnu11 -Wall -Wextra -o mediaplay src/player.c src/ui.c src/music.c src/video.c src/browser.c src/settings.c src/config.c src/input.c \
      $(pkg-config --cflags sdl2 SDL2_ttf SDL2_mixer) \
      -I/build/ffout/include \
      /build/ffout/lib/libavformat.a /build/ffout/lib/libavcodec.a /build/ffout/lib/libswresample.a \
      /build/ffout/lib/libswscale.a /build/ffout/lib/libavutil.a \
      $(pkg-config --libs sdl2 SDL2_ttf SDL2_mixer) -lm -lpthread -lz -Wl,--gc-sections \
    && strip mediaplay \
    && echo "--- dependencies ---" && readelf -d mediaplay | grep NEEDED \
    && ls -l mediaplay

FROM scratch AS out
COPY --from=build /build/mediaplay /mediaplay
