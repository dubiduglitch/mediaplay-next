/*
 * config.c - persistent config and resume positions for mediaplay-next.
 *
 * settings.cfg: key=value lines
 *   vol=80            music/video volume 0-100
 *   speed=1           playback speed index (0.5,1.0,1.5,2.0)
 *   sub=1             subtitles on/off
 *   fit=1             fit/fill (1=fit,0=fill)
 *
 * resume.tsv: seconds<TAB>path  (last 256 files)
 */

#include "mediaplay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_ENTRIES 256

/* ---------------------------------------------------------------- runtime config */
int config_vol = 100;
int config_speed_index = 1;   /* 1 = 1.0x */
int config_sub_on = 1;
int config_fit = 1;           /* 1 = fit (letterbox), 0 = fill */

/* ---------------------------------------------------------------- helpers */
static void config_ensure_dir(void) {
    mkdir(CONFIG_DIR, 0755);
}

static int config_get_int(const char *key, int def) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) return def;
    char line[256];
    int val = def;
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        if (strcmp(line, key) == 0) {
            val = atoi(eq + 1);
            break;
        }
    }
    fclose(f);
    return val;
}

static void config_set_int(const char *key, int val) {
    /* read all lines, replace or append */
    char lines[MAX_ENTRIES][256];
    int n = 0;
    FILE *f = fopen(CONFIG_FILE, "r");
    if (f) {
        while (n < MAX_ENTRIES && fgets(lines[n], sizeof lines[0], f)) n++;
        fclose(f);
    }
    int found = 0;
    for (int i = 0; i < n; i++) {
        char *eq = strchr(lines[i], '=');
        if (!eq) continue;
        *eq = 0;
        if (strcmp(lines[i], key) == 0) {
            *eq = '=';
            snprintf(lines[i], sizeof lines[0], "%s=%d\n", key, val);
            found = 1;
            break;
        }
    }
    if (!found && n < MAX_ENTRIES) {
        snprintf(lines[n], sizeof lines[0], "%s=%d\n", key, val);
        n++;
    }
    config_ensure_dir();
    f = fopen(CONFIG_FILE, "w");
    if (!f) return;
    for (int i = 0; i < n; i++) fputs(lines[i], f);
    fclose(f);
}

/* ---------------------------------------------------------------- load/save */
void config_load(void) {
    config_vol     = config_get_int("vol", 100);
    if (config_vol < 0) config_vol = 0;
    if (config_vol > 200) config_vol = 200;
    config_speed_index = config_get_int("speed", 1);
    if (config_speed_index < 0) config_speed_index = 0;
    if (config_speed_index > 3) config_speed_index = 3;
    config_sub_on  = config_get_int("sub", 1);
    config_fit     = config_get_int("fit", 1);
}

void config_save(void) {
    config_set_int("vol", config_vol);
    config_set_int("speed", config_speed_index);
    config_set_int("sub", config_sub_on);
    config_set_int("fit", config_fit);
}

/* ---------------------------------------------------------------- resume positions */
typedef struct { char path[1024]; int sec; } ResEntry;
static ResEntry resumes[MAX_ENTRIES];
static int nresumes = -1;  /* -1 = not loaded */

static void resumes_load(void) {
    if (nresumes >= 0) return;
    nresumes = 0;
    FILE *f = fopen(RESUME_FILE, "r");
    if (!f) return;
    char line[1024 + 32];
    while (nresumes < MAX_ENTRIES && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = 0;
        int sec = atoi(line);
        if (sec <= 0 || !tab[1]) continue;
        snprintf(resumes[nresumes].path, sizeof resumes[nresumes].path, "%s", tab + 1);
        resumes[nresumes].sec = sec;
        nresumes++;
    }
    fclose(f);
}

int config_resume_get(const char *path) {
    resumes_load();
    for (int i = 0; i < nresumes; i++)
        if (strcmp(resumes[i].path, path) == 0) return resumes[i].sec;
    return 0;
}

void config_resume_set(const char *path, int sec) {
    resumes_load();
    int i = -1;
    for (int j = 0; j < nresumes; j++)
        if (strcmp(resumes[j].path, path) == 0) { i = j; break; }
    if (sec <= 0) {
        if (i < 0) return;
        memmove(&resumes[i], &resumes[i + 1], sizeof(ResEntry) * (size_t)(nresumes - i - 1));
        nresumes--;
    } else {
        if (i >= 0) {
            if (resumes[i].sec == sec) return;
            ResEntry v = resumes[i];
            memmove(&resumes[i], &resumes[i + 1], sizeof(ResEntry) * (size_t)(nresumes - i - 1));
            resumes[nresumes - 1] = v;
            resumes[nresumes - 1].sec = sec;
        } else {
            if (nresumes == MAX_ENTRIES) {
                memmove(&resumes[0], &resumes[1], sizeof(ResEntry) * (MAX_ENTRIES - 1));
                nresumes--;
            }
            snprintf(resumes[nresumes].path, sizeof resumes[nresumes].path, "%s", path);
            resumes[nresumes].sec = sec;
            nresumes++;
        }
    }
    config_ensure_dir();
    FILE *f = fopen(RESUME_FILE, "w");
    if (!f) return;
    for (int i = 0; i < nresumes; i++) fprintf(f, "%d\t%s\n", resumes[i].sec, resumes[i].path);
    fclose(f);
}
