#!/usr/bin/env python3
"""
Sync Studio — PC-side media preparation tool for Medley (goodluckOS / Kennel).

Transcodes media to the Medley nominal profile, converts subtitles to UTF-8 SRT,
generates thumbnails, and produces a series manifest. Output is a folder ready to
copy onto the console SD card (/home/player/music, /home/player/videos).

Nominal profile (Medley 0.0.1):
  Video: H.264, <= 854x480, <= 30 fps, <= 1.5 Mb/s, MP4/MKV, AAC-LC stereo <= 160 kb/s
  Audio: MP3 / FLAC / OGG / WAV / M4A / AAC
  Subtitles: SRT (UTF-8), external, same basename as the video

Usage:
  python3 sync_studio.py prepare <media_folder> <output_folder>
  python3 sync_studio.py scan   <media_folder>        (report only, no changes)

Requires: ffmpeg, ffprobe on the PC (apt install ffmpeg on Debian)
"""

import argparse
import json
import os
import re
import subprocess
import sys

# Nominal profile limits
MAX_W = 854
MAX_H = 480
MAX_FPS = 30
MAX_VID_BITRATE = "1.5M"
MAX_AUD_BITRATE = "160k"


def run(cmd, check=True):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          text=True, check=check)


def probe(path):
    """Return ffprobe JSON for a file."""
    out = run(["ffprobe", "-v", "error", "-print_format", "json",
               "-show_format", "-show_streams", path])
    return json.loads(out.stdout)


def has_video_stream(info):
    return any(s.get("codec_type") == "video" for s in info.get("streams", []))


def has_audio_stream(info):
    return any(s.get("codec_type") == "audio" for s in info.get("streams", []))


def video_dimensions(info):
    for s in info.get("streams", []):
        if s.get("codec_type") == "video":
            return s.get("width", 0), s.get("height", 0)
    return 0, 0


def transcode_video(src, dst):
    """Transcode a video file to the Medley nominal profile."""
    info = probe(src)
    w, h = video_dimensions(info)
    need_scale = w > MAX_W or h > MAX_H
    scale = ""
    if need_scale:
        # fit inside 854x480 keeping aspect
        scale = f"scale='min({MAX_W},iw)':min({MAX_H},ih)':force_original_aspect_ratio=decrease"

    vf = scale if scale else "null"
    cmd = [
        "ffmpeg", "-y", "-i", src,
        "-map", "0:v", "-map", "0:a?",
        "-c:v", "libx264", "-profile:v", "main", "-level", "3.1",
        "-r", str(MAX_FPS), "-b:v", MAX_VID_BITRATE, "-maxrate", MAX_VID_BITRATE,
        "-bufsize", "3M",
    ]
    if need_scale:
        cmd += ["-vf", vf]
    cmd += ["-c:a", "aac", "-b:a", MAX_AUD_BITRATE, "-ac", "2",
            "-movflags", "+faststart", dst]
    run(cmd)
    return dst


def convert_subtitle(src, dst):
    """Convert a subtitle file to UTF-8 SRT."""
    # ffmpeg can convert many subtitle formats to srt
    cmd = ["ffmpeg", "-y", "-i", src, dst]
    run(cmd)
    return dst


def make_thumbnail(src, dst, w=240, h=360):
    """Extract a thumbnail from a video (or generate a music placeholder)."""
    info = probe(src)
    if has_video_stream(info):
        cmd = ["ffmpeg", "-y", "-i", src, "-ss", "00:00:02", "-frames:v", "1",
               "-vf", f"scale={w}:{h}:force_original_aspect_ratio=decrease,pad={w}:{h}:(ow-iw)/2:(oh-ih)/2",
               dst]
        run(cmd)
    else:
        # music: generate a simple note placeholder image
        cmd = ["ffmpeg", "-y", "-f", "lavfi", "-i",
               f"color=c=0x1a1918:{w}x{h}", "-vf",
               f"drawtext=text=M:fontfile=/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf:"
               f"fontsize={h//2}:fontcolor=white:x=(w-text_w)/2:y=(h-text_h)/2",
               dst]
        run(cmd)


def series_key(name):
    """Detect SxxExx / 1x02 / 'Saison 1 Épisode 2' patterns."""
    patterns = [
        r"[Ss](\d{1,2})[ ._-]?[Ee](\d{1,2})",
        r"(\d{1,2})x(\d{1,2})",
        r"[Ss]aison\s+(\d{1,2})[ ._-]?[\u00c9pisodep]\.?\s*(\d{1,2})",
    ]
    for p in patterns:
        m = re.search(p, name)
        if m:
            return int(m.group(1)), int(m.group(2))
    return None


def prepare_folder(src_folder, dst_folder):
    """Transcode all media in src_folder into dst_folder (music/ + videos/)."""
    os.makedirs(os.path.join(dst_folder, "music"), exist_ok=True)
    os.makedirs(os.path.join(dst_folder, "videos"), exist_ok=True)
    manifest = {"videos": [], "series": {}, "music": []}

    for name in sorted(os.listdir(src_folder)):
        src = os.path.join(src_folder, name)
        if not os.path.isfile(src):
            continue
        info = probe(src)
        if has_video_stream(info):
            dst = os.path.join(dst_folder, "videos", name)
            if not name.lower().endswith((".mp4", ".mkv")):
                dst = os.path.splitext(dst)[0] + ".mp4"
            transcode_video(src, dst)
            make_thumbnail(src, dst + ".thumb.jpg")
            key = series_key(name)
            entry = {"file": os.path.basename(dst), "duration": float(
                info.get("format", {}).get("duration", 0))}
            manifest["videos"].append(entry)
            if key:
                s, e = key
                manifest["series"].setdefault(s, []).append({"ep": e, "file": entry["file"]})
        elif has_audio_stream(info):
            dst = os.path.join(dst_folder, "music", name)
            # audio: copy or transcode to a supported container
            if name.lower().endswith((".mp3", ".flac", ".ogg", ".wav", ".m4a", ".aac")):
                subprocess.run(["cp", src, dst])
            else:
                dst = os.path.splitext(dst)[0] + ".m4a"
                run(["ffmpeg", "-y", "-i", src, "-c:a", "aac", "-b:a", MAX_AUD_BITRATE, dst])
            manifest["music"].append({"file": os.path.basename(dst)})
        # external subtitles: convert to UTF-8 SRT
        for ext in (".srt", ".ass", ".ssa"):
            if name.lower().endswith(ext):
                dst = os.path.splitext(src)[0] + ".srt"
                convert_subtitle(src, dst)

    with open(os.path.join(dst_folder, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
    return manifest


def main():
    ap = argparse.ArgumentParser(prog="sync_studio", description="Medley media prep tool")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p_prep = sub.add_parser("prepare", help="Transcode + organize a media folder")
    p_prep.add_argument("src")
    p_prep.add_argument("dst")
    p_scan = sub.add_parser("scan", help="Report media profile without changes")
    p_scan.add_argument("folder")
    args = ap.parse_args()

    if args.cmd == "prepare":
        m = prepare_folder(args.src, args.dst)
        print(json.dumps(m, indent=2))
    elif args.cmd == "scan":
        for name in sorted(os.listdir(args.folder)):
            path = os.path.join(args.folder, name)
            if not os.path.isfile(path):
                continue
            try:
                info = probe(path)
                print(name, "->", "video" if has_video_stream(info) else "audio",
                      video_dimensions(info) if has_video_stream(info) else "")
            except Exception as e:
                print(name, "->", "unreadable", e)


if __name__ == "__main__":
    main()
