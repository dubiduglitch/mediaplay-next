# Third-party components

mediaplay-next itself is the code in `src/`. The binary links these components:

| Component | Version | License | Linking |
|-----------|---------|---------|---------|
| SDL2 | console | zlib | dynamic |
| SDL2_ttf | console | zlib | dynamic |
| SDL2_mixer | console | zlib | dynamic |
| FFmpeg (libavformat, libavcodec, libswscale, libswresample, libavutil) | n6.1.1, minimal build | LGPL-2.1-or-later | static |
| dr_mp3 | public domain / MIT-0 | header only | static |

## What this means

- mediaplay-next's own code is **AGPL-3.0-or-later** (see LICENSE).
- FFmpeg is built **without** `--enable-gpl` and `--enable-nonfree`, so it stays LGPL.
- For private use on your own console nothing needs to be published.
