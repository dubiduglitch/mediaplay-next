/*
 * settings.c - in-app settings UI for mediaplay-next.
 *
 * Audio/subtitle language, playback speed, fit/fill, volume, subtitles on/off.
 */

#include "mediaplay.h"

#include <stdio.h>
#include <string.h>

static int settings_open_flag = 0;
static int settings_sel = 0;

static const int speed_values[] = {0, 1, 2, 3};   /* indices into speeds[] */
static const char *speed_labels[] = {"0.5x", "1.0x", "1.5x", "2.0x"};

void settings_open(void) {
    settings_open_flag = 1;
    settings_sel = 0;
    config_load();
}

void settings_close(void) {
    settings_open_flag = 0;
    config_save();
}

int settings_is_open(void) { return settings_open_flag; }

static const char *vol_label(void) {
    static char buf[32];
    snprintf(buf, sizeof buf, "%d%%", config_vol);
    return buf;
}

static const char *speed_label(void) {
    return speed_labels[config_speed_index];
}

static const char *fit_label(void) {
    return config_fit ? TX(X_FIT) : TX(X_FILL);
}

static const char *sub_label(void) {
    return config_sub_on ? TX(X_SUB_ON) : TX(X_SUB_OFF);
}

void settings_key(Key k) {
    switch (k) {
    case K_B: settings_close(); break;
    case K_UP:   if (settings_sel > 0) settings_sel--; break;
    case K_DOWN: if (settings_sel < 5) settings_sel++; break;
    case K_LEFT:
    case K_RIGHT:
        switch (settings_sel) {
        case 0: /* volume */
            if (k == K_LEFT) config_vol = config_vol > 0 ? config_vol - 10 : 0;
            else config_vol = config_vol < 200 ? config_vol + 10 : 200;
            Mix_VolumeMusic(config_vol * MIX_MAX_VOLUME / 200);
            break;
        case 1: /* speed */
            if (k == K_LEFT) config_speed_index = config_speed_index > 0 ? config_speed_index - 1 : 0;
            else config_speed_index = config_speed_index < 3 ? config_speed_index + 1 : 3;
            break;
        case 2: /* subtitles */
            config_sub_on = !config_sub_on;
            break;
        case 3: /* fit/fill */
            config_fit = !config_fit;
            break;
        }
        break;
    case K_A: /* toggle current row */
        switch (settings_sel) {
        case 2: config_sub_on = !config_sub_on; break;
        case 3: config_fit = !config_fit; break;
        }
        break;
    default: break;
    }
}

void settings_draw(void) {
    ui_draw_bg();
    ui_fill_rr(20, 14, 6, 30, 3, C_ACC, 255);
    ui_draw_text(f_big, TX(X_SETTINGS), 38, 10, C_TEXT, W - 230);
    ui_fill(20, 52, W - 40, 1, C_DIM, 70);

    struct { const char *key; const char *(*val)(void); SDL_Color col; } rows[5] = {
        {"Volume", vol_label, CC_MUSIC},
        {"Speed", speed_label, CC_VIDEO},
        {"Subtitles", sub_label, C_ACC},
        {"Fit/Fill", fit_label, C_ACC},
        {"Save & Back", NULL, C_GRN},
    };
    int y = 80;
    for (int i = 0; i < 5; i++) {
        int on = i == settings_sel;
        ui_fill_rr(30, y, W - 60, 44, 10, on ? C_SEL : C_CARD, 255);
        ui_fill_rr(30, y, 6, 44, 3, on ? CC_MUSIC : C_DIM, 255);
        ui_draw_text(f_mid, rows[i].key, 50, y + 8, C_TEXT, W - 150);
        if (rows[i].val) {
            ui_draw_text_right(f_mid, rows[i].val(), W - 40, y + 8, rows[i].col);
        } else {
            ui_draw_text_center(f_small, "press A to save", W / 2, y + 30, C_DIM, 0);
        }
        y += 54;
    }
    const char *k[6] = {"< >", "change", "A", "toggle/save", "B", TX(X_H_BACK)};
    ui_draw_hints(k, 3);
}
