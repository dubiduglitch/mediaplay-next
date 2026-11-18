# mediaplay-next — architecture

A proper music + video player for goodluckOS / Kennel on the GA36-MB V1.2-20260416
(Allwinner A23, Mali-400 MP2, 512 MB RAM, 640×480 screen).

## Design principles (addressing Mediabox's weaknesses)

1. **Modular** — UI, decoding, playback, and config are separate modules. No 2,300-line god file.
2. **Music first-class** — SDL2_mixer (already on the console) for MP3/FLAC/OGG/WAV; ID3 tags; album art; equalizer; gapless.
3. **Video proper** — statically-linked FFmpeg (bundled; console lacks FFmpeg libs). H.264/H.265/VP9/AV1. External subtitles (.srt/.ass). Audio/subtitle track selection. Playback speed. Resume.
4. **No books** — no PDF/MuPDF. Music + video only.
5. **In-app settings UI** — not a flat text file. Audio/subtitle language, speed, fit/fill, volume, theme, resume.
6. **Controller-optimized** — built for the GA36-MB gamepad via Kennel.
7. **Kennel-native** — `.puppy` launcher entry + `port.yaml` for goodluckOS-build integration.

## Module map

| Module | Responsibility |
|--------|---------------|
| `player.c` | Main loop, screen state machine, SDL init, input dispatch |
| `ui.c` | Drawing primitives, text cache, fonts, colors, icons |
| `music.c` | Music player (SDL2_mixer), playlist, ID3 tags, album art, equalizer |
| `video.c` | Video player (static FFmpeg), subtitles, track selection, speed |
| `browser.c` | File browser, folders, music/video roots |
| `settings.c` | Settings UI (audio/subtitle language, speed, fit/fill, volume, theme) |
| `config.c` | Persistent config (key/value file) + resume positions |

## Screens

```
HOME  → browse Music or Videos, open Settings/Help
BROWSE → folder list; A on music/video opens player
MUSIC  → now-playing bar, playlist, shuffle/repeat, equalizer
VIDEO  → video frame, subtitles overlay, HUD (progress, tracks, speed)
SETTINGS → audio/subtitle language, speed, fit/fill, volume, theme
HELP   → controls
```

## Data flow (video)

```
file → FFmpeg demux → audio packet queue → audio decoder → SDL audio device (master clock)
                        → video packet queue → video decoder → frame queue → SDL texture → screen
                                                      → subtitle decoder → text overlay
```

The main thread picks frames by the audio clock and drops late frames (A/V sync).

## Dependencies

| Lib | Source | Linking |
|-----|--------|---------|
| SDL2, SDL2_ttf, SDL2_mixer | console (already present) | dynamic |
| FFmpeg (avformat, avcodec, swscale, swresample, avutil) | built statically in Docker | static |
| Subtitle parsing | internal (.srt/.ass) | none |

Only SDL2, SDL2_ttf, SDL2_mixer, libm, libc remain dynamic — everything else is bundled.
