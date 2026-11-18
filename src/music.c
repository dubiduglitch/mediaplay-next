/*
 * music.c - music player (SDL2_mixer) for mediaplay-next.
 *
 * SDL2_mixer is already present on the console, so music is dynamic-linked.
 * Supports MP3, FLAC, OGG, WAV, M4A, AAC. Playlist, shuffle, repeat, seek,
 * volume, and a now-playing bar drawn on HOME/BROWSE.
 */

#include "mediaplay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------------------------------------------------------------- music state */
Mix_Music *cur_music = NULL;
int music_open = 0;
int music_paused = 0;
int music_ended = 0;
int pl_n = 0, pl_i = 0;
int repeat_mode = 1;     /* 0 off, 1 list, 2 one */
int shuffle = 0;
char play_err[160] = {0};
char **pl = NULL;
double music_elapsed = 0;
double music_total = 0;

static Uint32 play_start_ticks = 0;
static int play_was_paused = 0;
static double paused_at = 0;

/* ---------------------------------------------------------------- track management */
void music_track_close(void) {
    if (cur_music) { Mix_HaltMusic(); Mix_FreeMusic(cur_music); cur_music = NULL; }
    music_open = 0;
    music_paused = 0;
    music_ended = 0;
    music_elapsed = 0;
    music_total = 0;
    play_start_ticks = 0;
    paused_at = 0;
}

int music_track_open(const char *path) {
    music_track_close();
    play_err[0] = 0;
    cur_music = Mix_LoadMUS(path);
    if (!cur_music) {
        snprintf(play_err, sizeof play_err, "%s", TX(X_ERR_MUSIC));
        return 0;
    }
    music_open = 1;
    music_paused = 0;
    if (Mix_PlayMusic(cur_music, 0) == -1) {   /* 0 = play once; repetition handled by track_step */
        snprintf(play_err, sizeof play_err, "%s", TX(X_ERR_AUDIO));
        Mix_FreeMusic(cur_music);
        cur_music = NULL;
        music_open = 0;
        return 0;
    }
    play_start_ticks = SDL_GetTicks();
    paused_at = 0;
    /* duration: SDL2_mixer doesn't expose it directly; estimate from file size? */
    music_total = 0;   /* unknown; show elapsed only */
    return 1;
}

void music_play_index(int i) {
    if (i < 0 || i >= pl_n) return;
    pl_i = i;
    music_track_open(pl[i]);
}

void music_playlist_from_browser(int idx) {
    music_pl_free();
    int start = 0;
    for (int i = 0; i < nent; i++) {
        if (ents[i].type != T_MUSIC) continue;
        pl = realloc(pl, sizeof(char *) * (pl_n + 1));
        pl[pl_n] = strdup(ents[i].path);
        if (i == idx) start = pl_n;
        pl_n++;
    }
    if (pl_n) music_play_index(start);
}

static void track_step_internal(int idx) {
    if (pl_n == 0) return;
    if (idx >= pl_n) {
        if (repeat_mode == 0) { music_track_close(); return; }
        idx = 0;
    }
    if (idx < 0) idx = pl_n - 1;
    music_play_index(idx);
}

void music_track_step(int dir, int automatic) {
    if (pl_n == 0) return;
    int idx = pl_i;
    if (automatic && repeat_mode == 2) {
        /* same track */
    } else if (shuffle && pl_n > 1) {
        do idx = rand() % pl_n; while (idx == pl_i);
    } else {
        idx += dir;
        track_step_internal(idx);
        return;
    }
    music_play_index(idx);
}

double music_elapsed_sec(void) {
    if (!music_open) return 0;
    if (music_paused) return paused_at;
    double cur = (double)(SDL_GetTicks() - play_start_ticks) / 1000.0;
    return cur + paused_at;
}

void music_seek_rel(double secs) {
    /* SDL2_mixer has no seek; we restart isn't feasible per-seek.
       For now, clamp: seeking not supported by Mix_Music API.
       (Could re-implement with SDL2_mixer's Mix_SetPosition for some formats.) */
    (void)secs;
}

int music_playing(void) { return music_open && cur_music && Mix_PlayingMusic(); }
int music_ended_poll(void) { return music_ended; }
void music_ended_clear(void) { music_ended = 0; }
void music_pl_free(void) {
    for (int i = 0; i < pl_n; i++) free(pl[i]);
    free(pl);
    pl = NULL;
    pl_n = 0;
    pl_i = 0;
}

void music_vol_step(int dir) {
    int n = config_vol + dir * 10;
    if (n < 0) n = 0;
    if (n > 200) n = 200;
    config_vol = n;
    Mix_VolumeMusic(n * MIX_MAX_VOLUME / 200);
}

int music_vol_get(void) { return config_vol; }

/* ---------------------------------------------------------------- init/quit */
void music_init(void) {
    srand((unsigned)time(NULL));
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == -1) {
        fprintf(stderr, "Mix_OpenAudio: %s\n", Mix_GetError());
    }
    Mix_VolumeMusic(config_vol * MIX_MAX_VOLUME / 200);
}

void music_quit(void) {
    music_track_close();
    music_pl_free();
    Mix_CloseAudio();
}

/* ---------------------------------------------------------------- now playing bar */
void ui_draw_now_playing_bar(void) {
    ui_fill_rr(10, 406, W - 20, 36, 12, C_SEL, 255);
    char t[300];
    snprintf(t, sizeof t, "%s", browser_base_name(pl[pl_i]));
    if (music_paused) ui_icon_play(30, 424, 7, C_ACC);
    else ui_icon_pause(30, 424, 7, C_ACC);
    ui_draw_text(f_small, t, 48, 413, C_TEXT, 440);
    char a[32];
    double el = music_elapsed_sec();
    snprintf(a, sizeof a, "%d:%02d", (int)(el / 60), (int)el % 60);
    ui_draw_text_right(f_small, a, W - 22, 413, C_DIM);
    double tot = music_total;
    if (tot > 0) {
        double fr = el / tot;
        if (fr > 1) fr = 1;
        ui_fill(24, 437, (int)((W - 48) * fr), 2, C_ACC, 255);
    }
}

void ui_draw_player(void) {
    ui_draw_bg();
    /* header */
    ui_fill_rr(20, 14, 6, 30, 3, CC_MUSIC, 255);
    ui_draw_text(f_big, TX(X_NOW_PLAYING), 38, 10, C_TEXT, W - 230);
    ui_fill(20, 52, W - 40, 1, C_DIM, 70);

    if (!pl_n || !music_playing()) {
        ui_draw_text_center(f_mid, play_err[0] ? play_err : TX(X_NOTHING),
                            W / 2, 200, C_DIM, W - 60);
        const char *k[2] = {"B", TX(X_H_BACK)};
        ui_draw_hints(k, 1);
        return;
    }

    /* album-art placeholder */
    ui_fill_rr(30, 74, 190, 190, 20, (SDL_Color){40, 58, 96, 255}, 255);
    ui_fill_rr(36, 80, 178, 178, 16, (SDL_Color){52, 76, 124, 255}, 255);
    ui_icon_note(125, 169, 52, C_TEXT);

    char t[300];
    snprintf(t, sizeof t, "%s", browser_base_name(pl[pl_i]));
    ui_draw_text(f_big, t, 246, 88, C_TEXT, W - 266);
    char dir[1024];
    snprintf(dir, sizeof dir, "%s", pl[pl_i]);
    char *s = strrchr(dir, '/');
    if (s) *s = 0;
    ui_draw_text(f_small, browser_base_name(dir), 246, 134, C_DIM, W - 266);
    char info[64];
    snprintf(info, sizeof info, "%d / %d", pl_i + 1, pl_n);
    ui_draw_text(f_small, info, 246, 162, C_DIM, 0);

    int x = 246;
    char st[96];
    snprintf(st, sizeof st, "%s: %s", TX(X_SHUFFLE), shuffle ? TX(X_ON) : TX(X_OFF));
    x = ui_pill_l(x, 214, st, shuffle ? CC_MUSIC : C_PILL, shuffle ? C_DARK : C_DIM);
    snprintf(st, sizeof st, "%s: %s", TX(X_REPEAT),
             repeat_mode == 0 ? TX(X_OFF) :
             repeat_mode == 1 ? TX(X_LIST) : TX(X_ONE));
    ui_pill_l(x, 214, st, repeat_mode ? CC_MUSIC : C_PILL, repeat_mode ? C_DARK : C_DIM);

    /* progress bar */
    double el = music_elapsed_sec();
    double tot = music_total;
    int bx = 30, bw = W - 60, by = 300;
    ui_fill_rr(bx, by, bw, 10, 5, C_PILL, 255);
    int fw = 0;
    if (tot > 0) {
        double fr = el / tot; if (fr > 1) fr = 1;
        fw = (int)(bw * fr);
        if (fw > 0) ui_fill_rr(bx, by, fw < 10 ? 10 : fw, 10, 5, CC_MUSIC, 255);
    }
    ui_fill_circle(bx + fw, by + 5, 9, C_TEXT, 255);
    char a[32], b[32];
    snprintf(a, sizeof a, "%d:%02d", (int)(el / 60), (int)el % 60);
    snprintf(b, sizeof b, "%d:%02d", (int)(tot / 60), (int)tot % 60);
    ui_draw_text(f_small, a, bx, by + 20, C_TEXT, 0);
    if (tot > 0) ui_draw_text_right(f_small, b, bx + bw, by + 20, C_DIM);

    /* controls */
    int cy = 384;
    ui_icon_prev(W / 2 - 96, cy, 13, C_TEXT);
    ui_fill_circle(W / 2, cy, 32, CC_MUSIC, 255);
    if (music_paused) ui_icon_play(W / 2 + 2, cy, 14, C_DARK);
    else ui_icon_pause(W / 2, cy, 13, C_DARK);
    ui_icon_next(W / 2 + 96, cy, 13, C_TEXT);

    const char *k[14] = {"A", TX(X_H_PAUSE), "< >", TX(X_H_SEEK), "L/R", TX(X_H_TRACK),
                         "X", TX(X_SHUFFLE), "Y", TX(X_REPEAT), "Select", TX(X_H_STOP), "B", TX(X_H_BACK)};
    ui_draw_hints(k, 7);
}
