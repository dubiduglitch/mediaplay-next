/*
 * player.c - main loop, screen state machine, init/cleanup for mediaplay-next.
 */

#include "mediaplay.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---------------------------------------------------------------- globals */
SDL_Window   *win = NULL;
SDL_Renderer *R = NULL;
TTF_Font     *f_big, *f_mid, *f_small, *f_tiny;
SDL_GameController *pad = NULL;
int   running = 1;
int   dirty = 1;
Screen scr = S_HOME;

/* ---------------------------------------------------------------- forward declarations */
static void handle_key(Key k);
static void draw_screen(void);

/* ---------------------------------------------------------------- init */
static int sdl_init(void) {
    SDL_SetHint("SDL_HINT_RENDER_SCALE_QUALITY", "linear");
    SDL_SetHint("SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS", "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
            fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
            return 0;
        }
    }
    TTF_Init();
    win = SDL_CreateWindow("mediaplay", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                           W, H, 0);
    if (!win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 0; }
    R = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!R) R = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!R) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return 0; }
    SDL_RenderSetLogicalSize(R, W, H);
    SDL_ShowCursor(SDL_DISABLE);
    input_open_pad();
    return 1;
}

static void fonts_init(void) {
    const char *ff = access(FONT_MAIN, R_OK) == 0 ? FONT_MAIN : FONT_ALT;
    f_big   = TTF_OpenFont(ff, 30);
    f_mid   = TTF_OpenFont(ff, 22);
    f_small = TTF_OpenFont(ff, 18);
    f_tiny  = TTF_OpenFont(ff, 14);
}

/* ---------------------------------------------------------------- key handling */
static void handle_key(Key k) {
    dirty = 1;

    if (settings_is_open()) { settings_key(k); return; }
    if (scr == S_HELP) { if (k == K_B || k == K_A || k == K_START) scr = S_HOME; return; }

    switch (scr) {
    case S_VIDEO:
        video_key(k);
        if (!video_is_open()) scr = S_BROWSE;
        return;
    case S_MUSIC:
        switch (k) {
        case K_A: case K_START:
            if (music_open) { music_paused = !music_paused; Mix_PauseMusic(music_paused); }
            break;
        case K_LEFT:  music_seek_rel(-10); break;
        case K_RIGHT: music_seek_rel(10); break;
        case K_L:
            if (music_elapsed_sec() > 3) music_seek_rel(-1e9); else music_track_step(-1, 0);
            break;
        case K_R: music_track_step(1, 0); break;
        case K_X: shuffle = !shuffle; break;
        case K_Y: repeat_mode = (repeat_mode + 1) % 3; break;
        case K_SELECT: music_track_close(); scr = S_BROWSE; break;
        case K_B: scr = S_BROWSE; break;
        default: break;
        }
        return;
    case S_SETTINGS:
        settings_key(k);
        return;
    case S_BROWSE:
    case S_HOME:
        switch (k) {
        case K_UP:
            if (scr == S_HOME) { if (sel > 0) sel--; else sel = nent - 1; }
            else { /* browse list */ }
            break;
        case K_DOWN:
            if (scr == S_HOME) { if (sel < nent - 1) sel++; else sel = 0; }
            break;
        case K_L: case K_LEFT:
            if (k == K_L && scr == S_HOME) { sel -= 1; if (sel < 0) sel = 0; }
            break;
        case K_R: case K_RIGHT:
            if (k == K_R && scr == S_HOME) { sel += 1; if (sel >= nent) sel = nent - 1; }
            break;
        case K_A:
            if (scr == S_HOME) {
                switch (ents[sel].type) {
                case T_MUSIC: scr = S_BROWSE; cat = 0; browser_open_category(0); break;
                case T_VIDEO: scr = S_BROWSE; cat = 1; browser_open_category(1); break;
                case T_SETTINGS: settings_open(); break;
                default: break;
                }
            } else {
                /* browse activate */
                if (nent == 0) break;
                Entry *e = &ents[sel];
                switch (e->type) {
                case T_DIR: {
                    char p[1024]; snprintf(p, sizeof p, "%s", e->path);
                    browser_enter_dir(p);
                    break;
                }
                case T_MUSIC:
                    music_playlist_from_browser(sel);
                    if (music_open || play_err[0]) { scr = S_MUSIC; }
                    break;
                case T_VIDEO:
                    if (video_open(e->path)) scr = S_VIDEO;
                    break;
                default: break;
                }
            }
            break;
        case K_B:
            if (scr == S_HOME) running = 0;
            else browser_go_up();
            break;
        case K_START:
            if (scr == S_BROWSE && (music_open || pl_n)) { scr = S_MUSIC; }
            break;
        case K_Y:
            if (scr == S_HOME) { /* language toggle placeholder */ }
            break;
        default: break;
        }
        return;
    default: break;
    }
}

/* ---------------------------------------------------------------- direction repeat */
static void fire_dirs(int m) {
    if (m & 1) handle_key(K_UP);
    if (m & 2) handle_key(K_DOWN);
    if (m & 4) handle_key(K_LEFT);
    if (m & 8) handle_key(K_RIGHT);
}

/* ---------------------------------------------------------------- draw */
static void draw_screen(void) {
    if (settings_is_open()) { settings_draw(); return; }
    switch (scr) {
    case S_HOME:   ui_draw_home(); break;
    case S_BROWSE: ui_draw_browse(); break;
    case S_MUSIC:  ui_draw_player(); break;
    case S_VIDEO:  video_draw(); break;
    case S_HELP:   break;
    case S_SETTINGS: settings_draw(); break;
    }
}

/* ---------------------------------------------------------------- main loop */
int mediaplay_main(void) {
    if (!sdl_init()) return 1;
    fonts_init();
    ui_init();
    config_load();
    music_init();
    browser_show_home();

    int held = 0;
    Uint32 next_rep = 0, last_tick = 0;

    while (running) {
        SDL_Event ev;
        int timeout = (scr == S_VIDEO) ? 5 : (held ? 20 : 250);
        if (SDL_WaitEventTimeout(&ev, timeout)) {
            do { input_handle_event(&ev); } while (SDL_PollEvent(&ev));
        }
        Uint32 now = SDL_GetTicks();
        int m = input_read_dirs();
        int fresh = m & ~held;
        if (fresh) { fire_dirs(fresh); next_rep = now + 350; }
        else if (m && now >= next_rep) { fire_dirs(m); next_rep = now + 85; }
        held = m;

        if (scr == S_VIDEO) video_tick();
        if (music_open && Mix_PlayingMusic() == 0 && !music_paused && !music_ended) {
            music_track_step(1, 1);
            dirty = 1;
        }
        if ((scr == S_HOME || scr == S_BROWSE) && now - last_tick >= 500) {
            last_tick = now;
            dirty = 1;
        }
        if (dirty) {
            dirty = 0;
            draw_screen();
            SDL_RenderPresent(R);
        }
    }

    if (scr == S_VIDEO && video_is_open()) config_resume_set(video_path, (int)video_get_clock());
    video_close();
    music_quit();
    ui_quit();
    if (pad) SDL_GameControllerClose(pad);
    TTF_Quit();
    SDL_DestroyRenderer(R);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}


