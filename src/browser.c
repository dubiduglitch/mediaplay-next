/*
 * browser.c - file browser, categories, roots, home/browse screens for mediaplay-next.
 *
 * Handles folder navigation and the home/browse list drawing. The "now playing"
 * bar (needs music state) is drawn by music.c / player.c.
 */

#include "mediaplay.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ---------------------------------------------------------------- browser state */
Entry *ents = NULL;
int nent = 0, ncap = 0;
int sel = 0, topi = 0;
char cwd[1024] = {0};
int cat = 0;                 /* 0 music, 1 videos */
char roots[2][1024] = {{0}, {0}};
int nroots = 0;
char msg[160] = {0};
int home_sel = 0;

/* ---------------------------------------------------------------- entry list */
void browser_clear(void) {
    nent = 0;
    sel = 0;
    topi = 0;
}

Entry *browser_add(void) {
    if (nent == ncap) {
        ncap = ncap ? ncap * 2 : 128;
        ents = realloc(ents, sizeof(Entry) * ncap);
        if (!ents) { fprintf(stderr, "out of memory\n"); exit(1); }
    }
    Entry *e = &ents[nent++];
    memset(e, 0, sizeof *e);
    return e;
}

static int cmp_ent(const void *a, const void *b) {
    const Entry *x = a, *y = b;
    int dx = x->type == T_DIR, dy = y->type == T_DIR;
    if (dx != dy) return dy - dx;
    return strcasecmp(x->name, y->name);
}

void browser_sort(void) {
    qsort(ents, nent, sizeof(Entry), cmp_ent);
}

/* ---------------------------------------------------------------- helpers */
static int is_dir_path(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int is_music_name(const char *n) {
    size_t nlen = strlen(n);
    static const char *exts[] = {".mp3", ".flac", ".ogg", ".wav", ".m4a", ".aac", NULL};
    for (int i = 0; exts[i]; i++) {
        size_t elen = strlen(exts[i]);
        if (nlen > elen && strcasecmp(n + nlen - elen, exts[i]) == 0) return 1;
    }
    return 0;
}

static int is_video_name(const char *n) {
    size_t nlen = strlen(n);
    static const char *exts[] = {".mp4", ".m4v", ".mov", ".mkv", ".avi", ".webm", NULL};
    for (int i = 0; exts[i]; i++) {
        size_t elen = strlen(exts[i]);
        if (nlen > elen && strcasecmp(n + nlen - elen, exts[i]) == 0) return 1;
    }
    return 0;
}

static const char *base_name(const char *p) {
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

const char *browser_base_name(const char *p) {
    return base_name(p);
}

/* ---------------------------------------------------------------- navigation */
void browser_show_home(void) {
    static const EType items[] = {T_MUSIC, T_VIDEO, T_SETTINGS, T_HELP};
    browser_clear();
    for (int i = 0; i < 4; i++) {
        Entry *e = browser_add();
        e->type = items[i];
    }
    sel = home_sel < 4 ? home_sel : 0;
    cwd[0] = 0;
}

static const char *rm[2] = {"/home/player/music", "/media/external/music"};
static const char *rv[2] = {"/home/player/videos", "/media/external/videos"};

void browser_open_category(int c) {
    cat = c;
    nroots = 0;
    msg[0] = 0;
    for (int i = 0; i < 2; i++) {
        const char *p = c == 0 ? rm[i] : rv[i];
        if (is_dir_path(p)) snprintf(roots[nroots++], sizeof roots[0], "%s", p);
    }
    if (nroots == 0) {
        browser_clear();
        cwd[0] = 0;
        snprintf(msg, sizeof msg, TX(X_NO_FOLDER),
                 c == 0 ? rm[0] : rv[0]);
    } else if (nroots == 1) {
        browser_enter_dir(roots[0]);
    } else {
        browser_show_roots();
    }
}

void browser_enter_dir(const char *path) {
    browser_clear();
    snprintf(cwd, sizeof cwd, "%s", path);
    DIR *d = opendir(path);
    if (!d) { snprintf(msg, sizeof msg, "%s", TX(X_ERR_DIR)); return; }
    struct dirent *de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char full[1024];
        snprintf(full, sizeof full, "%s/%s", path, de->d_name);
        EType t;
        if (is_dir_path(full)) t = T_DIR;
        else if (cat == 0 && is_music_name(de->d_name)) t = T_MUSIC;
        else if (cat == 1 && is_video_name(de->d_name)) t = T_VIDEO;
        else continue;
        Entry *e = browser_add();
        snprintf(e->name, sizeof e->name, "%s", de->d_name);
        snprintf(e->path, sizeof e->path, "%s", full);
        e->type = t;
        if (t != T_DIR) {
            char *dot = strrchr(e->name, '.');
            if (dot) *dot = 0;
        }
    }
    closedir(d);
    browser_sort();
}

void browser_show_roots(void) {
    browser_clear();
    cwd[0] = 0;
    for (int i = 0; i < nroots; i++) {
        Entry *e = browser_add();
        e->name[0] = 0;   /* label chosen by language at draw time */
        snprintf(e->path, sizeof e->path, "%s", roots[i]);
        e->type = T_DIR;
    }
}

int browser_is_root(const char *p) {
    for (int i = 0; i < nroots; i++)
        if (strcmp(roots[i], p) == 0) return 1;
    return 0;
}

void browser_select_path(const char *path) {
    for (int i = 0; i < nent; i++)
        if (strcmp(ents[i].path, path) == 0) { sel = i; return; }
}

void browser_go_up(void) {
    if (!cwd[0]) { browser_show_home(); return; }
    if (browser_is_root(cwd)) {
        if (nroots > 1) {
            char prev[1024];
            snprintf(prev, sizeof prev, "%s", cwd);
            browser_show_roots();
            browser_select_path(prev);
        } else {
            browser_show_home();
        }
        return;
    }
    char prev[1024], parent[1024];
    snprintf(prev, sizeof prev, "%s", cwd);
    snprintf(parent, sizeof parent, "%s", cwd);
    char *s = strrchr(parent, '/');
    if (s && s != parent) *s = 0;
    browser_enter_dir(parent);
    browser_select_path(prev);
}

#define ROW_H 40
#define HOME_H 66
#define LIST_Y 62

/* ---------------------------------------------------------------- draw helpers */
static SDL_Color cat_color(void) {
    return cat == 0 ? CC_MUSIC : CC_VIDEO;
}

static int list_clamp(void) {
    extern Screen scr;
    int bottom = (scr == S_HOME || scr == S_BROWSE) ? 400 : 440;
    int rv = (bottom - LIST_Y) / ROW_H;
    if (nent == 0) { sel = topi = 0; return rv; }
    if (sel < 0) sel = 0;
    if (sel >= nent) sel = nent - 1;
    if (sel < topi) topi = sel;
    if (sel >= topi + rv) topi = sel - rv + 1;
    if (topi < 0) topi = 0;
    return rv;
}

static void draw_header(const char *title, SDL_Color col) {
    ui_fill_rr(20, 14, 6, 30, 3, col, 255);
    ui_draw_text(f_big, title, 38, 10, C_TEXT, W - 230);
    ui_fill(20, 52, W - 40, 1, C_DIM, 70);
}

static void home_info(EType t, const char **title, const char **sub, SDL_Color *col) {
    switch (t) {
    case T_MUSIC:  *title = TX(X_MUSIC);  *sub = TX(X_SUB_MUSIC);  *col = CC_MUSIC; break;
    case T_VIDEO:  *title = TX(X_VIDEOS); *sub = TX(X_SUB_VIDEOS); *col = CC_VIDEO; break;
    case T_SETTINGS: *title = TX(X_SETTINGS); *sub = "Configure playback"; *col = C_ACC; break;
    default:       *title = TX(X_HELP);   *sub = "Controls and info"; *col = C_ACC; break;
    }
}

void ui_draw_home(void) {
    extern int home_sel;
    ui_draw_bg();
    ui_fill_rr(20, 14, 6, 30, 3, C_ACC, 255);
    ui_draw_text(f_big, "mediaplay", 38, 10, C_TEXT, 300);
    ui_draw_text_right(f_small, "Music  ·  Videos", W - 24, 20, C_DIM);
    ui_fill(20, 52, W - 40, 1, C_DIM, 70);
    for (int i = 0; i < nent; i++) {
        int x = 20, y = LIST_Y + i * HOME_H, w = W - 40, h = 60, on = i == sel;
        const char *title, *sub;
        SDL_Color col;
        home_info(ents[i].type, &title, &sub, &col);
        if (on) {
            ui_fill_rr(x - 2, y - 2, w + 4, h + 4, 16, col, 255);
            ui_fill_rr(x, y, w, h, 14, (SDL_Color){34, 48, 80, 255}, 255);
        } else {
            ui_fill_rr(x, y, w, h, 14, C_CARD, 255);
        }
        ui_fill_rr(x + 10, y + 8, 44, 44, 12, col, 255);
        int cx = x + 32, cy = y + 30;
        switch (ents[i].type) {
        case T_MUSIC:  ui_icon_note(cx, cy + 1, 14, C_DARK); break;
        case T_VIDEO:  ui_icon_video(cx, cy, 15, C_DARK, col); break;
        case T_SETTINGS: ui_fill_rr(cx - 10, cy - 10, 20, 20, 3, C_DARK, 255); break;
        default:       ui_draw_text_center(f_mid, "?", cx, cy - 15, C_DARK, 0); break;
        }
        ui_draw_text(f_mid, title, x + 70, y + 6, C_TEXT, w - 150);
        ui_draw_text(f_tiny, sub, x + 70, y + 35, C_DIM, w - 150);
        if (on) ui_icon_play(x + w - 26, y + 30, 7, col);
    }
    const char *k[6] = {"A", TX(X_H_OPEN), "B", TX(X_H_EXIT),
                        "Start", TX(X_H_NOWPLAYING)};
    ui_draw_hints(k, 3);
}

void ui_draw_browse(void) {
    extern Screen scr;
    ui_draw_bg();
    SDL_Color cc = cat == 0 ? CC_MUSIC : CC_VIDEO;
    char title[300];
    if (!cwd[0]) snprintf(title, sizeof title, "%s",
                          cat == 0 ? TX(X_MUSIC) : TX(X_VIDEOS));
    else snprintf(title, sizeof title, "%s", base_name(cwd));
    draw_header(title, cc);
    if (nent > 0) {
        char n[48];
        snprintf(n, sizeof n, TX(X_ITEMS), nent);
        ui_draw_text_right(f_small, n, W - 24, 20, C_DIM);
    }
    int rv = list_clamp();
    if (nent == 0) {
        ui_draw_text(f_mid, msg[0] ? msg : TX(X_NO_FILES), 28, LIST_Y + 24, C_DIM, W - 56);
    }
    for (int i = 0; i < rv && topi + i < nent; i++) {
        int idx = topi + i;
        int y = LIST_Y + i * ROW_H;
        if (idx == sel) {
            ui_fill_rr(14, y, W - 36, ROW_H - 4, 10, C_SEL, 255);
            ui_fill_rr(14, y + 7, 4, ROW_H - 18, 2, cc, 255);
        }
        ui_draw_badge(26, y + 3, ents[idx].type);
        const char *label = ents[idx].name;
        if (!cwd[0] && ents[idx].type == T_DIR && nroots > 1)
            label = strncmp(ents[idx].path, "/media/external", 15) == 0 ? TX(X_EXTERNAL) : TX(X_INTERNAL);
        int lw = W - 120;
        if (ents[idx].type == T_VIDEO) {
            int sc = config_resume_get(ents[idx].path);
            if (sc >= 10) {
                char tm[32];
                snprintf(tm, sizeof tm, "%d:%02d", sc / 60, sc % 60);
                ui_draw_text_right(f_tiny, tm, W - 28, y + 10, CC_VIDEO);
                lw = W - 200;
            }
        }
        ui_draw_text(f_mid, label, 74, y + 4, C_TEXT, lw);
    }
    if (nent > rv) {
        int th_ = rv * ROW_H - 4, bh = th_ * rv / nent;
        if (bh < 18) bh = 18;
        int by = LIST_Y + (th_ - bh) * topi / (nent - rv);
        ui_fill_rr(W - 14, LIST_Y, 4, th_, 2, C_DIM, 80);
        ui_fill_rr(W - 14, by, 4, bh, 2, cc, 255);
    }
    const char *k[8] = {"A", TX(X_H_OPEN), "B", TX(X_H_BACK), "L/R", TX(X_H_PAGE),
                        "Start", TX(X_H_NOWPLAYING)};
    ui_draw_hints(k, 4);
}
