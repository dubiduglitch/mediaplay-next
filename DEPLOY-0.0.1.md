# Medley 0.0.1 — Deploy & Test on the Console

Prerequisites (on your PC): the `dist/mediabox` armhf binary from the Docker build.
Prerequisites (on the console): goodluckOS + Kennel, a test SD card (not your main one).

## 1. Build the armhf binary (PC)

```bash
cd ~/mediaplay-next
# first build downloads Debian + builds FFmpeg under QEMU (~15-25 min)
docker buildx build --platform linux/arm/v7 --target out --output type=local,dest=dist .
# result: dist/mediaplay
```

CI is also wired: `.github/workflows/build.yml` builds on push to `main` and attaches
`medley-armhf` as a workflow artifact.

## 2. Install on the console (read-only to system — everything under /home/player)

On the console, copy files (USB OTG may not work on V1.2 A23 — use a card reader
or copy onto the SD card from your PC):

```bash
# on the SD card (mounted on your PC or the console)
mkdir -p /home/player/ports/mediaplay
cp dist/mediaplay /home/player/ports/mediaplay/mediaplay
chmod +x /home/player/ports/mediaplay/mediaplay

cp mediaplay.puppy /home/player/.local/share/applications/mediaplay.puppy
# icon (optional — create a small 64x64 PNG, or Kennel falls back to a default)
cp <your-icon.png> /home/player/.local/share/icons/mediaplay.png
```

## 3. Test media

Put test files in:
- `/home/player/music/` — MP3, FLAC, OGG, WAV, M4A, AAC
- `/home/player/videos/` — MP4, MKV, AVI, WebM (H.264/HEVC/VP9/AV1)
- external card: `/media/external/music`, `/media/external/videos`
- subtitles: `movie.srt` next to `movie.mp4` (external SRT, UTF-8)

## 4. Launch

Open Kennel → Media category → "Media Player" (A). Or run directly:

```bash
/home/player/ports/mediaplay/mediaplay
```

## 5. 0.0.1 test checklist (P0)

| # | Test | Pass criteria |
|---|------|---------------|
| T1 | Launch from Kennel | ≤ 2 s to first interactive screen |
| T2 | Music plays (MP3/FLAC/OGG/WAV) | sound ≤ 0.5 s after A |
| T3 | Music pause/seek/next/prev | works |
| T4 | Music keeps playing while browsing | now-playing bar shows |
| T5 | Video plays (H.264 MP4/MKV ≤ 854×480) | picture + sound in sync |
| T6 | Video pause/seek/volume/fill-fit | works |
| T7 | External SRT subtitles show/hide | K_S toggle works |
| T8 | Resume position remembered | reopening a video resumes |
| T9 | Exit back to Kennel | B / Select returns ≤ 1 s |
| T10 | No crash/gap on 8 h loop | (endurance, later) |

Report results with timestamps so we can compare against Mediabox v1.2.0 baseline.

## 6. Rollback

Uninstall = delete `/home/player/ports/mediaplay/` and
`/home/player/.local/share/applications/mediaplay.puppy`. Nothing else changes.
