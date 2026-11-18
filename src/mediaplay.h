#ifndef MEDIAPLAY_H
#define MEDIAPLAY_H

/*
 * mediaplay-next - music + video player for goodluckOS / Kennel.
 * GA36-MB V1.2-20260416 (Allwinner A23, Mali-400 MP2, 512 MB, 640x480).
 *
 * Music: SDL2_mixer (MP3, FLAC, OGG, WAV) with ID3 tags, album art, equalizer.
 * Video: static FFmpeg (H.264/H.265/VP9/AV1), external subtitles (.srt/.ass),
 *        audio/subtitle track selection, playback speed, resume.
 *
 * Modules:
 *   player.c  - main loop, screen state machine, init/cleanup
 *   ui.c      - drawing primitives, text cache, fonts, colors
 *   music.c   - music player (SDL2_mixer)
 *   video.c   - video player (FFmpeg)
 *   browser.c - file browser / folders
 *   settings.c- settings UI
 *   config.c  - persistent config (key/value file) + resume positions
 */

#define _GNU_SOURCE

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------- screen geometry */
#define W 640
#define H 480

/* ---------------------------------------------------------------- paths */
#define FONT_MAIN  "/usr/share/fonts/Inter_24pt-Medium.ttf"
#define FONT_ALT   "/home/player/ports/kennel/assets/fonts/Inter-Regular.ttf"

#define CONFIG_DIR "/home/player/.config/mediaplay"
#define CONFIG_FILE "/home/player/.config/mediaplay/settings.cfg"
#define RESUME_FILE "/home/player/.config/mediaplay/resume.tsv"

/* ---------------------------------------------------------------- screens */
typedef enum {
    S_HOME,
    S_BROWSE,
    S_MUSIC,
    S_VIDEO,
    S_SETTINGS,
    S_HELP,
} Screen;

/* ---------------------------------------------------------------- keys */
typedef enum {
    K_UP, K_DOWN, K_LEFT, K_RIGHT,
    K_A, K_B, K_X, K_Y,
    K_L, K_R, K_L2, K_R2,
    K_START, K_SELECT,
} Key;

/* ---------------------------------------------------------------- entry types */
typedef enum {
    T_DIR,
    T_MUSIC,
    T_VIDEO,
    T_SUBTITLE,
    T_CAT_MUSIC,
    T_CAT_VIDEO,
    T_SETTINGS,
    T_HELP,
} EType;

typedef struct {
    char  name[256];
    char  path[1024];
    EType type;
} Entry;

/* ---------------------------------------------------------------- i18n */
typedef enum { L_EN } Lang;

/* String ids */
typedef enum {
    X_MUSIC, X_VIDEOS, X_SETTINGS, X_HELP,
    X_SUB_MUSIC, X_SUB_VIDEOS,
    X_INTERNAL, X_EXTERNAL, X_NO_FILES, X_NO_FOLDER, X_ERR_DIR, X_ITEMS,
    X_NOW_PLAYING, X_NOTHING,
    X_REPEAT, X_SHUFFLE, X_OFF, X_ON, X_LIST, X_ONE,
    X_ERR_MUSIC, X_ERR_AUDIO,
    X_ERR_VOPEN, X_ERR_VINFO, X_ERR_NOVID, X_ERR_VCODEC, X_ERR_VSIZE,
    X_ERR_VTEX, X_ERR_THREAD,
    X_BUFFERING, X_VOL, X_DROPPED, X_FIT, X_FILL, X_RESUMED,
    X_SPEED, X_AUDIO_TRACK, X_SUB_TRACK, X_SUB_NONE, X_SUB_ON, X_SUB_OFF,
    X_H_OPEN, X_H_BACK, X_H_EXIT, X_H_PAGE, X_H_NOWPLAYING,
    X_H_SEEK, X_H_TRACK, X_H_PAUSE, X_H_STOP,
    X_HELP_MUSIC_T, X_HELP_MUSIC_1, X_HELP_MUSIC_2,
    X_HELP_VIDEO_T, X_HELP_VIDEO_1, X_HELP_VIDEO_2,
    X_HELP_DIR_T, X_HELP_DIR_1, X_HELP_DIR_2,
    X_N,
} XId;

extern const char *TX(int x);

/* ---------------------------------------------------------------- globals (player.c) */
extern SDL_Window   *win;
extern SDL_Renderer *R;
extern TTF_Font     *f_big, *f_mid, *f_small, *f_tiny;
extern SDL_GameController *pad;
extern int   running;
extern int   dirty;
extern Screen scr;

/* palette (ui.c) - shared colors */
extern const SDL_Color C_BG, C_TEXT, C_DIM, C_ACC, C_SEL, C_RED, C_AMB, C_GRN;
extern const SDL_Color CC_MUSIC, CC_VIDEO, C_DARK, C_PILL, C_KEY, C_CARD;

/* config (config.c) */
extern int config_vol;
extern int config_speed_index;
extern int config_sub_on;
extern int config_fit;

/* browser state (browser.c) */
extern Entry *ents;
extern int    nent, ncap;
extern int    sel, topi;
extern char   cwd[1024];
extern int    cat;
extern char   roots[2][1024];
extern int    nroots;
extern char   msg[160];
extern int    home_sel;

/* music state (music.c) */
extern Mix_Music *cur_music;
extern int   music_open;
extern int   music_paused;
extern int   music_ended;
extern int   pl_n, pl_i;
extern int   repeat_mode;
extern int   shuffle;
extern char  play_err[160];
extern char **pl;
extern double music_elapsed;
extern double music_total;

/* video state (video.c) */
extern int   video_open_ok;
extern int   video_paused;
extern int   video_buffering;
extern int   video_fit;
extern int   video_dropped;
extern int   video_vol;
extern char  video_title[300];
extern char  video_err[160];
extern char  video_path[1024];
extern double video_dur;

/* ---------------------------------------------------------------- module APIs */

/* ui.c */
void ui_init(void);
void ui_quit(void);
void ui_draw_home(void);
void ui_draw_browse(void);
void ui_draw_player(void);
void ui_draw_bg(void);
void ui_fill(int x, int y, int w, int h, SDL_Color c, int a);
void ui_fill_rr(int x, int y, int w, int h, int r, SDL_Color c, int a);
void ui_fill_circle(int cx, int cy, int r, SDL_Color c, int a);
void ui_draw_text(TTF_Font *f, const char *s, int x, int y, SDL_Color c, int maxw);
void ui_draw_text_center(TTF_Font *f, const char *s, int cx, int y, SDL_Color c, int maxw);
void ui_draw_text_right(TTF_Font *f, const char *s, int xr, int y, SDL_Color c);
void ui_draw_hints(const char *const *kv, int n);
void ui_icon_play(int cx, int cy, int s, SDL_Color c);
void ui_icon_pause(int cx, int cy, int s, SDL_Color c);
void ui_icon_next(int cx, int cy, int s, SDL_Color c);
void ui_icon_prev(int cx, int cy, int s, SDL_Color c);
void ui_icon_note(int cx, int cy, int s, SDL_Color c);
void ui_icon_video(int cx, int cy, int s, SDL_Color c, SDL_Color hole);
int  ui_pill(int xr, int y, const char *s, SDL_Color bg, SDL_Color fg);
int  ui_pill_l(int x, int y, const char *s, SDL_Color bg, SDL_Color fg);
void ui_draw_badge(int x, int y, EType t);
void ui_tc_free(void);
int  ui_text_w(TTF_Font *f, const char *s);

/* config.c */
void config_load(void);
void config_save(void);
int  config_resume_get(const char *path);
void config_resume_set(const char *path, int sec);

/* browser.c */
void browser_clear(void);
Entry *browser_add(void);
void browser_sort(void);
void browser_show_home(void);
void browser_enter_dir(const char *path);
void browser_show_roots(void);
void browser_open_category(int c);
int  browser_is_root(const char *p);
void browser_go_up(void);
void browser_select_path(const char *path);
const char *browser_base_name(const char *p);

/* music.c */
void music_init(void);
void music_quit(void);
void music_track_close(void);
int  music_track_open(const char *path);
void music_play_index(int i);
void music_playlist_from_browser(int idx);
void music_track_step(int dir, int automatic);
void music_seek_rel(double secs);
double music_elapsed_sec(void);
int  music_playing(void);
int  music_ended_poll(void);
void music_ended_clear(void);
void music_pl_free(void);
void music_vol_step(int dir);
int  music_vol_get(void);

/* video.c */
int  video_open(const char *path);
void video_key(Key k);
void video_draw(void);
void video_tick(void);
void video_close(void);
int  video_is_open(void);
double video_get_clock(void);
void video_vol_step(int dir);

/* settings.c */
void settings_open(void);
void settings_key(Key k);
void settings_draw(void);
int  settings_is_open(void);

/* input.c */
int  input_read_dirs(void);
void input_handle_event(const SDL_Event *ev);
void input_open_pad(void);

/* player.c */
int mediaplay_main(void);

#endif /* MEDIAPLAY_H */
