#!/bin/sh
# console-probe.sh — Medley Phase 1 probe for goodluckOS / Kennel (GA36-MB).
# READ-ONLY: collects device info and writes device-profile.json + raw log.
# Never touches boot, MBR, partitions, or system files.
#
# Usage on the console:
#   sh console-probe.sh > probe-raw.log 2>&1
#   cat device-profile.json
#
set -u

OUT="${1:-device-profile.json}"
RAW="${2:-probe-raw.log}"

exec >"$RAW" 2>&1

echo "============================================================"
echo "Medley console probe — $(date)"
echo "==========================================================="

say() { echo "## $*"; }
cmd() { echo "--- $*"; eval "$*" 2>&1 || echo "[cmd failed or not available: $*]"; }

say "Identity"
cmd "uname -a"
cmd "cat /etc/os-release 2>/dev/null || true"
cmd "cat /proc/cpuinfo"
cmd "cat /proc/meminfo"
cmd "uptime"

say "CPU frequencies & governors"
cmd "cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq 2>/dev/null || true"
cmd "cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor 2>/dev/null || true"

say "Graphics / GPU"
cmd "ls -la /dev/dri 2>/dev/null || echo 'no /dev/dri'"
cmd "ls -la /dev/fb* 2>/dev/null || echo 'no /dev/fb'"
cmd "dmesg 2>/dev/null | grep -iE 'lima|mali|cedrus|sun4i|drm|gpu' | head -40 || true"

say "Audio"
cmd "cat /proc/asound/cards 2>/dev/null || echo 'no alsa cards'"
cmd "amixer contents 2>/dev/null || echo 'no amixer'"
cmd "ls /dev/snd/* 2>/dev/null || echo 'no /dev/snd'"

say "Video hardware (decode)"
cmd "ls -la /dev/video* /dev/media* 2>/dev/null || echo 'no video4linux/media devices'"
cmd "dmesg 2>/dev/null | grep -iE 'cedrus|v4l2|vdec|h264|hevc|vp9' | head -30 || true"

say "Libraries"
cmd "ldconfig -p 2>/dev/null | grep -iE 'sdl|ffmpeg|avcodec|avformat|swscale|swresample|avutil|ass|freetype|harfbuzz|fribidi|asound' || true"
cmd "which ffmpeg ffprobe mpv mediabox 2>/dev/null || echo 'no ffmpeg/mpv/mediabox binaries'"

say "Storage & SD"
cmd "mount 2>/dev/null | grep -iE 'mmc|sd|/media|/home' || true"
cmd "lsblk 2>/dev/null || true"
cmd "ls -la /media/external 2>/dev/null || echo 'no /media/external'"
cmd "ls -la /home/player 2>/dev/null || echo 'no /home/player'"

say "SD speed (read-only, sequential 100 MB)"
cmd "dd if=/dev/zero of=/tmp/.probe-write-test bs=1M count=100 conv=fsync 2>&1 | tail -2 || true"
cmd "rm -f /tmp/.probe-write-test"

say "USB & radio"
cmd "lsusb 2>/dev/null || echo 'no lsusb'"
cmd "dmesg 2>/dev/null | tail -20 || true"
cmd "rfkill list 2>/dev/null || echo 'no rfkill'"
cmd "hciconfig 2>/dev/null || echo 'no hciconfig'"

say "Power & backlight & inputs"
cmd "ls /sys/class/power_supply 2>/dev/null || echo 'no power_supply'"
cmd "ls /sys/class/backlight 2>/dev/null || echo 'no backlight'"
cmd "cat /sys/class/power_supply/*/capacity 2>/dev/null || true"
cmd "cat /sys/class/power_supply/*/status 2>/dev/null || true"
cmd "evtest 2>/dev/null <<'EOF' || true
EOF
"
cmd "ls /dev/input/* 2>/dev/null || echo 'no /dev/input'"

say "SDL2 probe (tiny C program, read-only)"
cat > /tmp/.probe-sdl.c <<'CEOF'
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
int main(void){
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER)!=0){printf("SDL_INIT_FAIL:%s\n",SDL_GetError());return 1;}
    printf("SDL_OK:%s\n",SDL_GetError());
    int n=SDL_NumJoysticks(); printf("JOYSTICKS:%d\n",n);
    TTF_Init(); Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,2048);
    printf("TTF_MIXER_OK\n");
    Mix_CloseAudio(); TTF_Quit(); SDL_Quit();
    return 0;
}
CEOF
cc -o /tmp/.probe-sdl /tmp/.probe-sdl.c $(pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_mixer 2>/dev/null) 2>/dev/null && /tmp/.probe-sdl || echo "sdl probe build/skip (no dev libs on host path)"; rm -f /tmp/.probe-sdl /tmp/.probe-sdl.c

say "End of probe — $(date)"

# Build device-profile.json from what we can (best effort, best-effort parsing)
python3 - "$OUT" "$RAW" <<'PYEOF' 2>/dev/null || true
import json, re, sys, os
out, raw = sys.argv[1], sys.argv[2]
text = open(raw, errors="ignore").read() if os.path.exists(raw) else ""
prof = {"probe_date": __import__("datetime").datetime.now().isoformat(), "source_raw": raw}

m = re.search(r"PRETTY_NAME=\"([^\"]+)\"", text); 
if m: prof["os"] = m.group(1)
m = re.search(r"model name\s+:\s+(.+)", text)
if m: prof["cpu"] = m.group(1).strip()
m = re.search(r"MemTotal:\s+(\d+)", text)
if m: prof["mem_kb"] = int(m.group(1))
if "lima" in text.lower(): prof["gpu_driver"] = "lima"
if "mali" in text.lower(): prof["gpu_driver"] = "mali"
if "cedrus" in text.lower(): prof["video_hw"] = "cedrus"
if "/dev/dri" in text: prof["dri"] = True
if "/dev/video" in text: prof["video4linux"] = True
libs = []
for lib in ["SDL2","ffmpeg","avcodec","libass","freetype","libasound"]:
    if lib in text: libs.append(lib)
prof["libs_detected"] = libs
open(out,"w").write(json.dumps(prof, indent=2))
print("wrote", out)
PYEOF

echo "------------------------------------------------------------"
echo "Probe complete. Review $RAW and $OUT"
