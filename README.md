# mediaplay-next

A proper music + video player for **goodluckOS / Kennel** on the **GA36-MB V1.2-20260416**
(Allwinner A23, Mali-400 MP2, 512 MB RAM, 640×480 screen).

Music and video only — no books, no PDF, no fuss.

## Features

**Music**
- MP3, FLAC, OGG, WAV, M4A, AAC via SDL2_mixer (already on the console)
- Playlist with shuffle / repeat (off / list / one)
- Now-playing bar on the home/browse screens (music keeps playing while you browse)
- Volume control (hardware mixer via SDL2_mixer)

**Video**
- MKV, MP4, AVI, WebM via statically-linked FFmpeg (H.264, HEVC, VP9, AV1)
- External subtitles (.srt/.ass) — on/off toggle
- Audio / subtitle track selection
- Playback speed (0.5×, 1×, 1.5×, 2×)
- Fit / fill (letterbox vs stretch)
- Resume position per file
- A/V sync with frame dropping

**UI**
- Controller-optimized for the GA36-MB gamepad
- 640×480, dirty-flag redraw (low CPU on the A23)
- In-app settings (volume, speed, subtitles, fit/fill) — no text-file hacking
- English / Turkish strings

## Controls

| Screen | Keys |
|--------|------|
| Home | Up/Down choose, A open, B exit, Start now-playing |
| Browse | A open/enter, B up, L/R page, Start now-playing |
| Music | A pause, ←/→ seek 10 s, L/R prev/next, X shuffle, Y repeat, Select stop, B back |
| Video | A pause, ←/→ seek, L/R 1 min, ↑/↓ volume, X fit/fill, S subtitles, T audio track, B quit |
| Settings | Up/Down choose, ←/→ change, A toggle, B save+back |

## Build

The binary is cross-compiled for `linux/arm/v7` in Docker (Debian bookworm) with
statically-linked FFmpeg. Only SDL2, SDL2_ttf, SDL2_mixer remain dynamic (already on the console).

```bash
docker buildx build --platform linux/arm/v7 --target out --output type=local,dest=dist .
# -> dist/mediaplay
```

Or use the goodluckOS Buildroot cross-toolchain (same as the rest of the OS).

## Install on the console

1. Copy `dist/mediaplay` to `/home/player/ports/mediaplay/mediaplay`
2. Copy `mediaplay.puppy` to `/home/player/.local/share/applications/mediaplay.puppy`
3. Restart Kennel (or launch from the Media category)

## License

GNU GPL-3.0-or-later (see LICENSE). FFmpeg is LGPL-2.1+; SDL2_mixer is zlib/libpng-style.
See NOTICE.md.
