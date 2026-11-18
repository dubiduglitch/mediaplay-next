/*
 * ui.c - drawing primitives, text cache, colors, icons for mediaplay-next.
 *
 * Pure rendering. Knows nothing about playlist, browser, or video state.
 */

#include "mediaplay.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- fonts */
TTF_Font *f_big, *f_mid, *f_small, *f_tiny;

/* ---------------------------------------------------------------- palette */
const SDL_Color C_BG   = {18, 19, 24, 255};
const SDL_Color C_TEXT = {236, 236, 242, 255};
const SDL_Color C_DIM  = {140, 143, 156, 255};
const SDL_Color C_ACC  = {88, 166, 255, 255};
const SDL_Color C_SEL  = {36, 56, 88, 255};
const SDL_Color C_RED  = {232, 92, 92, 255};
const SDL_Color C_AMB  = {236, 178, 70, 255};
const SDL_Color C_GRN  = {120, 220, 140, 255};

const SDL_Color CC_MUSIC = {88, 166, 255, 255};
const SDL_Color CC_VIDEO = {96, 200, 130, 255};
const SDL_Color C_DARK   = {14, 16, 24, 255};
const SDL_Color C_PILL   = {52, 58, 82, 255};
const SDL_Color C_KEY    = {70, 77, 104, 255};
const SDL_Color C_CARD   = {28, 31, 46, 255};

/* ---------------------------------------------------------------- background */
static SDL_Texture *bg_tex;

static void bg_init(void) {
    Uint32 px[H];
    for (int y = 0; y < H; y++) {
        double t = (double)y / (H - 1);
        int r = (int)(22 + (14 - 22) * t);
        int g = (int)(26 + (16 - 26) * t);
        int b = (int)(42 + (30 - 42) * t);
        px[y] = 0xFF000000u | ((Uint32)r << 16) | ((Uint32)g << 8) | (Uint32)b;
    }
    bg_tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 1, H);
    if (bg_tex) SDL_UpdateTexture(bg_tex, NULL, px, 4);
}

void ui_draw_bg(void) {
    if (bg_tex) SDL_RenderCopy(R, bg_tex, NULL, NULL);
    else ui_fill(0, 0, W, H, C_BG, 255);
}

/* ---------------------------------------------------------------- fill primitives */
void ui_fill(int x, int y, int w, int h, SDL_Color c, int a) {
    if (w <= 0 || h <= 0) return;
    SDL_SetRenderDrawBlendMode(R, a < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(R, c.r, c.g, c.b, (Uint8)a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(R, &r);
}

void ui_fill_rr(int x, int y, int w, int h, int r, SDL_Color c, int a) {
    if (w <= 0 || h <= 0) return;
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    if (r <= 0) { ui_fill(x, y, w, h, c, a); return; }
    ui_fill(x, y + r, w, h - 2 * r, c, a);
    for (int i = 0; i < r; i++) {
        double dy = (double)(r - i) - 0.5;
        int dx = r - (int)(sqrt((double)r * r - dy * dy) + 0.5);
        ui_fill(x + dx, y + i, w - 2 * dx, 1, c, a);
        ui_fill(x + dx, y + h - 1 - i, w - 2 * dx, 1, c, a);
    }
}

void ui_fill_circle(int cx, int cy, int r, SDL_Color c, int a) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)(sqrt((double)r * r - (double)dy * dy) + 0.5);
        ui_fill(cx - dx, cy + dy, 2 * dx + 1, 1, c, a);
    }
}

/* ---------------------------------------------------------------- text cache */
#define TC_N 128
typedef struct {
    TTF_Font *f;
    SDL_Color c;
    char s[200];
    SDL_Texture *t;
    int w, h;
    Uint32 used;
} TCache;

static TCache tcache[TC_N];
static Uint32 tc_clock;

void ui_tc_free(void) {
    for (int i = 0; i < TC_N; i++)
        if (tcache[i].t) { SDL_DestroyTexture(tcache[i].t); tcache[i].t = NULL; }
}

static int text_w(TTF_Font *f, const char *s) {
    int w = 0, h = 0;
    if (f && s[0]) TTF_SizeUTF8(f, s, &w, &h);
    return w;
}

int ui_text_w(TTF_Font *f, const char *s) { return text_w(f, s); }

void ui_draw_text(TTF_Font *f, const char *s, int x, int y, SDL_Color c, int maxw) {
    if (!f || !s) return;
    char buf[512];
    snprintf(buf, sizeof buf, "%s", s);
    if (maxw > 0 && text_w(f, buf) > maxw) {
        size_t n = strlen(buf);
        while (n > 0) {
            n--;
            while (n > 0 && (buf[n] & 0xC0) == 0x80) n--;
            buf[n] = 0;
            char t[520];
            snprintf(t, sizeof t, "%s...", buf);
            if (text_w(f, t) <= maxw) { snprintf(buf, sizeof buf, "%s", t); break; }
        }
    }
    if (!buf[0]) return;
    int cacheable = (int)strlen(buf) < (int)sizeof tcache[0].s;
    if (cacheable) {
        for (int i = 0; i < TC_N; i++) {
            TCache *e = &tcache[i];
            if (e->t && e->f == f && e->c.r == c.r && e->c.g == c.g && e->c.b == c.b &&
                strcmp(e->s, buf) == 0) {
                e->used = ++tc_clock;
                SDL_Rect d = {x, y, e->w, e->h};
                SDL_RenderCopy(R, e->t, NULL, &d);
                return;
            }
        }
    }
    SDL_Surface *sf = TTF_RenderUTF8_Blended(f, buf, c);
    if (!sf) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(R, sf);
    if (t) {
        SDL_Rect d = {x, y, sf->w, sf->h};
        SDL_RenderCopy(R, t, NULL, &d);
        if (cacheable) {
            TCache *v = &tcache[0];
            for (int i = 0; i < TC_N; i++) {
                if (!tcache[i].t) { v = &tcache[i]; break; }
                if (tcache[i].used < v->used) v = &tcache[i];
            }
            if (v->t) SDL_DestroyTexture(v->t);
            v->f = f; v->c = c;
            snprintf(v->s, sizeof v->s, "%s", buf);
            v->t = t; v->w = sf->w; v->h = sf->h; v->used = ++tc_clock;
        } else {
            SDL_DestroyTexture(t);
        }
    }
    SDL_FreeSurface(sf);
}

void ui_draw_text_right(TTF_Font *f, const char *s, int xr, int y, SDL_Color c) {
    ui_draw_text(f, s, xr - ui_text_w(f, s), y, c, 0);
}

void ui_draw_text_center(TTF_Font *f, const char *s, int cx, int y, SDL_Color c, int maxw) {
    int w = ui_text_w(f, s);
    if (maxw > 0 && w > maxw) w = maxw;
    ui_draw_text(f, s, cx - w / 2, y, c, maxw);
}

/* ---------------------------------------------------------------- icons */
void ui_icon_play(int cx, int cy, int s, SDL_Color c) {
    for (int i = -s; i <= s; i++) {
        int w = (int)((double)s * 1.5 * (1.0 - (double)abs(i) / (double)s)) + 1;
        ui_fill(cx - s / 2, cy + i, w, 1, c, 255);
    }
}

void ui_icon_pause(int cx, int cy, int s, SDL_Color c) {
    int bw = s * 2 / 3 > 3 ? s * 2 / 3 : 3, gap = s / 2 + 2;
    ui_fill_rr(cx - gap / 2 - bw, cy - s, bw, 2 * s, 2, c, 255);
    ui_fill_rr(cx + gap / 2, cy - s, bw, 2 * s, 2, c, 255);
}

void ui_icon_next(int cx, int cy, int s, SDL_Color c) {
    for (int i = -s; i <= s; i++) {
        int w = (int)((double)s * 1.5 * (1.0 - (double)abs(i) / (double)s)) + 1;
        ui_fill(cx - s, cy + i, w, 1, c, 255);
    }
    ui_fill(cx + s - 2, cy - s, 3, 2 * s + 1, c, 255);
}

void ui_icon_prev(int cx, int cy, int s, SDL_Color c) {
    for (int i = -s; i <= s; i++) {
        int w = (int)((double)s * 1.5 * (1.0 - (double)abs(i) / (double)s)) + 1;
        ui_fill(cx + s - w, cy + i, w, 1, c, 255);
    }
    ui_fill(cx - s, cy - s, 3, 2 * s + 1, c, 255);
}

void ui_icon_note(int cx, int cy, int s, SDL_Color c) {
    int r = s * 33 / 100 > 3 ? s * 33 / 100 : 3;
    int sw = s / 7 > 2 ? s / 7 : 2;
    int h1x = cx - s / 2, h1y = cy + s / 2;
    int h2x = cx + s / 2, h2y = cy + s / 4;
    int x1 = h1x + r - sw, x2 = h2x + r - sw, top = cy - s;
    ui_fill_circle(h1x, h1y, r, c, 255);
    ui_fill_circle(h2x, h2y, r, c, 255);
    ui_fill(x1, top, sw, h1y - top, c, 255);
    ui_fill(x2, top, sw, h2y - top, c, 255);
    ui_fill_rr(x1, top, x2 - x1 + sw, s * 3 / 10 > 3 ? s * 3 / 10 : 3, 2, c, 255);
}

void ui_icon_video(int cx, int cy, int s, SDL_Color c, SDL_Color hole) {
    ui_fill_rr(cx - s, cy - s * 7 / 10, 2 * s, s * 14 / 10, 4, c, 255);
    ui_icon_play(cx, cy, s / 2 > 2 ? s / 2 : 2, hole);
}

/* ---------------------------------------------------------------- pills / hints */
int ui_pill_l(int x, int y, const char *s, SDL_Color bg, SDL_Color fg) {
    int w = ui_text_w(f_tiny, s) + 18;
    ui_fill_rr(x, y, w, 22, 11, bg, 255);
    ui_draw_text(f_tiny, s, x + 9, y + 2, fg, 0);
    return x + w + 8;
}

int ui_pill(int xr, int y, const char *s, SDL_Color bg, SDL_Color fg) {
    int w = ui_text_w(f_tiny, s) + 18;
    ui_pill_l(xr - w, y, s, bg, fg);
    return xr - w - 8;
}

void ui_draw_hints(const char *const *kv, int n) {
    int wid[10], rowi[10], rx[10], kw[10];
    if (n > 10) n = 10;
    int x = 20, row = 0;
    for (int i = 0; i < n; i++) {
        kw[i] = ui_text_w(f_tiny, kv[2 * i]) + 14;
        if (kw[i] < 24) kw[i] = 24;
        wid[i] = kw[i] + 6 + ui_text_w(f_tiny, kv[2 * i + 1]) + 18;
        if (x + wid[i] - 18 > W - 20 && x > 20 && row < 1) { row++; x = 20; }
        rowi[i] = row; rx[i] = x; x += wid[i];
    }
    int rows = row + 1;
    for (int i = 0; i < n; i++) {
        int y = H - 30 - (rows - 1 - rowi[i]) * 26;
        ui_fill_rr(rx[i], y, kw[i], 22, 7, C_KEY, 255);
        ui_draw_text_center(f_tiny, kv[2 * i], rx[i] + kw[i] / 2, y + 2, C_TEXT, 0);
        ui_draw_text(f_tiny, kv[2 * i + 1], rx[i] + kw[i] + 6, y + 2, C_DIM, 0);
    }
}

/* ---------------------------------------------------------------- badge */
void ui_draw_badge(int x, int y, EType t) {
    switch (t) {
    case T_DIR:
        ui_fill_rr(x, y + 3, 16, 9, 3, (SDL_Color){214, 160, 60, 255}, 255);
        ui_fill_rr(x, y + 7, 34, 19, 5, C_AMB, 255);
        break;
    case T_MUSIC:
        ui_fill_rr(x, y + 3, 34, 22, 6, CC_MUSIC, 255);
        ui_draw_text_center(f_tiny, "MP3/FLAC/OGG", x + 17, y + 5, C_DARK, 0);
        break;
    case T_VIDEO:
        ui_fill_rr(x, y + 3, 34, 22, 6, CC_VIDEO, 255);
        ui_draw_text_center(f_tiny, "VIDEO", x + 17, y + 5, C_DARK, 0);
        break;
    default:
        ui_fill_rr(x, y + 3, 34, 22, 6, C_ACC, 255);
        break;
    }
}

/* ---------------------------------------------------------------- init/quit */
void ui_init(void) {
    bg_init();
}

void ui_quit(void) {
    if (bg_tex) { SDL_DestroyTexture(bg_tex); bg_tex = NULL; }
    ui_tc_free();
}

/* ---------------------------------------------------------------- strings (i18n) */
static const char *const STR[X_N][2] = {
    [X_MUSIC]       = {"Music", "M\u00fazik"},
    [X_VIDEOS]      = {"Videos", "Videolar"},
    [X_SETTINGS]    = {"Settings", "Ayarlar"},
    [X_HELP]        = {"Help", "Yard\u0131m"},
    [X_SUB_MUSIC]   = {"Music player", "M\u00fazik oynat\u0131c\u0131"},
    [X_SUB_VIDEOS]  = {"Video player", "Video oynat\u0131c\u0131"},
    [X_INTERNAL]    = {"Internal card", "Dahili kart"},
    [X_EXTERNAL]    = {"External card", "Harici kart"},
    [X_NO_FILES]    = {"No files in this folder", "Bu klas\u00f6rde dosya yok"},
    [X_NO_FOLDER]   = {"Folder not found: %s", "Klas\u00f6r bulunamad\u0131: %s"},
    [X_ERR_DIR]     = {"Could not open folder", "Klas\u00f6r ac\u0131lamad\u0131"},
    [X_ITEMS]       = {"%d items", "%d \u00f6\u011fe"},
    [X_NOW_PLAYING] = {"Now playing", "\u00c7al\u0131yor"},
    [X_NOTHING]     = {"Nothing is playing", "\u00c7al\u0131\u015fan yok"},
    [X_REPEAT]      = {"Repeat", "Tekrar"},
    [X_SHUFFLE]     = {"Shuffle", "Kar\u0131\u015f\u0131k"},
    [X_OFF]         = {"Off", "Kapal\u0131"},
    [X_ON]          = {"On", "A\u00e7\u0131k"},
    [X_LIST]        = {"List", "Liste"},
    [X_ONE]         = {"One", "Tek"},
    [X_ERR_MUSIC]   = {"Could not open music", "M\u00fazik ac\u0131lamad\u0131"},
    [X_ERR_AUDIO]   = {"Could not open audio device", "Ses ayg\u0131t\u0131 ac\u0131lamad\u0131"},
    [X_ERR_VOPEN]   = {"Could not open video", "Video ac\u0131lamad\u0131"},
    [X_ERR_VINFO]   = {"Could not read video info", "Video bilgisi okunamad\u0131"},
    [X_ERR_NOVID]   = {"No video stream found", "Video ak\u0131\u015f\u0131 yok"},
    [X_ERR_VCODEC]  = {"Unsupported video format", "Bu video bi\u00e7imi desteklenmiyor"},
    [X_ERR_VSIZE]   = {"Invalid video size", "Ge\u00e7ersiz video boyutu"},
    [X_ERR_VTEX]    = {"Could not create video texture", "Video doku\u00b8su olu\u00b8turulamad\u0131"},
    [X_ERR_THREAD]  = {"Could not start decoder thread", "\u00c7\u00f6z\u00fcc\u00fc i\u015f par\u00e7ac\u0131\u011f\u0131 ba\u015flat\u0131lamad\u0131"},
    [X_BUFFERING]   = {"Buffering...", "Y\u00fckleniyor..."},
    [X_VOL]         = {"Vol", "Ses"},
    [X_DROPPED]     = {"Dropped", "Atlanan"},
    [X_FIT]         = {"Fit", "S\u0131\u011fd\u0131r"},
    [X_FILL]        = {"Fill", "Doldur"},
    [X_RESUMED]     = {"Continuing from %s   (Y: start over)", "%s konumundan devam   (Y: ba\u015ftan)"},
    [X_SPEED]       = {"Speed", "H\u0131z"},
    [X_AUDIO_TRACK] = {"Audio track", "Ses par\u00e7a\u00e7\u0131"},
    [X_SUB_TRACK]   = {"Subtitle track", "Altyaz\u0131 par\u00e7a\u00e7\u0131"},
    [X_SUB_NONE]    = {"None", "Yok"},
    [X_SUB_ON]      = {"On", "A\u00e7\u0131k"},
    [X_SUB_OFF]     = {"Off", "Kapal\u0131"},
    [X_H_OPEN]      = {"Open", "A\u00e7"},
    [X_H_BACK]      = {"Back", "Geri"},
    [X_H_EXIT]      = {"Exit", "\u00c7\u0131k\u0131\u015f"},
    [X_H_PAGE]      = {"Page", "Sayfa"},
    [X_H_NOWPLAYING]= {"Now playing", "\u00c7al\u0131yor"},
    [X_H_SEEK]      = {"Seek", "Sar"},
    [X_H_TRACK]     = {"Track", "Par\u00e7a"},
    [X_H_PAUSE]     = {"Pause", "Duraklat"},
    [X_H_STOP]      = {"Stop", "Durdur"},
    [X_HELP_MUSIC_T] = {"Music (MP3/FLAC/OGG/WAV)", "M\u00fazik (MP3/FLAC/OGG/WAV)"},
    [X_HELP_MUSIC_1] = {"A pause    < > seek 10 s    L/R prev/next",
                        "A duraklat    < > 10 sn sar    L/R \u00f6nceki/sonraki"},
    [X_HELP_MUSIC_2] = {"X shuffle    Y repeat    Select stop    B back",
                        "X kar\u0131\u015f\u0131k    Y tekrar    Select durdur    B geri"},
    [X_HELP_VIDEO_T] = {"Videos (MKV/MP4/AVI)", "Videolar (MKV/MP4/AVI)"},
    [X_HELP_VIDEO_1] = {"A pause    < > seek 10 s    L/R 1 min    +/- speed",
                        "A duraklat    < > 10 sn sar    L/R 1 dk    +/- h\u0131z"},
    [X_HELP_VIDEO_2] = {"^ v volume    X fit/fill    T audio track    S subtitle    B quit",
                        "^ v ses    X s\u0131\u011fd\u0131r/doldur    T ses dili    S altyaz\u0131    B \u00e7\u0131k"},
    [X_HELP_DIR_T]   = {"Folders", "Klas\u00f6rler"},
    [X_HELP_DIR_1]   = {"/home/player/music    /home/player/videos",
                        "/home/player/music    /home/player/videos"},
    [X_HELP_DIR_2]   = {"Same folders on external card: /media/external/...",
                        "Harici kartta ayn\u0131 klas\u00f6rler: /media/external/..."},
};

const char *TX(int x) {
    extern Lang lang;
    (void)lang;
    return STR[x][0];
}
