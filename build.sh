#!/usr/bin/env bash
# Local build helper for mediaplay-next (x86_64 host, for syntax check only;
# real armhf builds happen in Docker/goodluckOS Buildroot).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
src="$here/src"

echo "== syntax check (no SDL2/FFmpeg dev libs on host, so compile-only) =="
gcc -fsyntax-only -std=gnu11 -Wall -Wextra \
    "$src/player.c" "$src/ui.c" "$src/music.c" "$src/video.c" \
    "$src/browser.c" "$src/settings.c" "$src/config.c" "$src/input.c" \
    2>&1 | head -60 || true

echo "== docker armhf build =="
docker buildx build --platform linux/arm/v7 --target out --output type=local,dest=dist "$here"
