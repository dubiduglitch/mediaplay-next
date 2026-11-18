# ADR-001: License, name, and project scope

## Status
Accepted

## Context
The user wants a music + video media player ("Medley") for goodluckOS / Kennel on the GA36-MB V1.2-20260416 (Allwinner A23, Mali-400 MP2, 512 MB, 640×480). The user chose the license, name, and build environment.

## Decision
- **License:** GPL-3.0-or-later (user choice, 2026-11-18). Not AGPL (Mediabox's license) — user explicitly selected GPL-3.0.
- **App name:** Medley (user choice). Repo: `dubiduglitch/mediaplay-next`.
- **UI language:** English only (user choice). No Arabic/Hebrew subtitle support (S3 stays P1).
- **Scope:** Music + video only. No books/PDF (unlike Mediabox).
- **Build PC:** Linux (Debian 13 trixie, x86_64, Python 3.13.5, gcc 14.2). Sync Studio will be Python 3 + ffmpeg, cross-platform but developed on this Linux box.
- **Default assumptions (to validate):** second SD slot = yes; headphones = wired only (no Bluetooth; S7 conditionnal); no auto-launch at console boot; content ~500 videos / 5000 tracks, 720p H.264 nominal.

## Consequences
- Source lives under GPL-3.0; FFmpeg stays LGPL-2.1+ (built without --enable-gpl/--enable-nonfree); SDL2/SDL2_ttf/SDL2_mixer remain zlib.
- NOTICE.md and LICENSE updated; port.yaml license field set to GPL-3.0-or-later.
- Sync Studio is a separate PC-side CLI tool (Python), not part of the console binary.

## ADRs to follow
- ADR-002: media engine (FFmpeg vs libmpv, software vs hardware decode)
- ADR-003: subtitle engine (libass vs internal SRT parser)
- ADR-004: library storage (SQLite vs flat files)
- ADR-005: audio output (SDL2_mixer vs direct ALSA) and headphone detection
