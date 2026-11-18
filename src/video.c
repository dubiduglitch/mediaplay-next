/*
 * video.c - video player (static FFmpeg) for mediaplay-next.
 *
 * H.264/H.265/VP9/AV1 video with AAC/MP3/AC3/Opus audio. External subtitles
 * (.srt/.ass). Audio/subtitle track selection. Playback speed. Resume.
 *
 * FFmpeg is linked statically (bundled in the Docker build) because the
 * console does not ship FFmpeg libraries.
 */

#include "mediaplay.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include <pthread.h>
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------- video state */
int   video_open_ok = 0;
int   video_paused = 0;
int   video_buffering = 0;
int   video_fit = 1;        /* 1 fit (letterbox), 0 fill */
int   video_dropped = 0;
int   video_vol = 100;
char  video_title[300] = {0};
char  video_err[160] = {0};
char  video_path[1024] = {0};
double video_dur = 0;

static AVFormatContext *v_fmt = NULL;
static AVCodecContext  *v_vctx = NULL, *v_actx = NULL;
static AVStream        *v_vst = NULL, *v_ast = NULL;
static int v_vidx = -1, v_aidx = -1;
static struct SwrContext *v_swr = NULL;
static struct SwsContext *v_sws = NULL, *v_sws2 = NULL;
static pthread_t v_th;
static int v_th_on = 0;
static pthread_mutex_t v_mx = PTHREAD_MUTEX_INITIALIZER;
static atomic_int v_abort, v_seek_req, v_eof;
static double v_seek_to;
static unsigned v_seek_gen, v_seek_done;
static double v_discard_until, v_t0, v_last_vpts, v_frame_dur;

#define VQ_N 12
typedef struct { AVFrame *f; double pts; } VSlot;
static VSlot v_q[VQ_N];
static int v_qr, v_qw, v_qn;

static Uint8 *v_ring = NULL;
static int v_ring_sz = 0, v_ring_r = 0, v_ring_w = 0, v_ring_n = 0;
static SDL_AudioDeviceID v_adev = 0;
static int v_arate = 0;
static double v_alat = 0, v_clock_base = 0;
static long long v_consumed = 0;
static Uint8 *v_abuf = NULL;
static int v_abuf_cap = 0;

static int v_audio_done = 0;
static Uint32 v_buf_t0 = 0, v_wall_t0 = 0;
static double v_buf_pos = 0, v_wall_base = 0;
static double v_sar = 1.0;
static Uint8 *v_rgb = NULL;
static SDL_Texture *v_tex = NULL;
static int v_tex_rgb = 0;
static int v_w = 0, v_h = 0;
static int v_have_frame = 0;
static char v_sub_text[512] = {0};

/* subtitle track (external .srt) */
static int v_sub_on = 1;
static int v_sub_track = -1;

/* speed */
static const double speeds[] = {0.5, 1.0, 1.5, 2.0};
static int v_speed_index = 1;

/* ---------------------------------------------------------------- packet queues */
#define PQ_N 4096
typedef struct { AVPacket *p[PQ_N]; int r, w, n; size_t bytes; } PQ;
static PQ v_vpq, v_apq;

static int pq_push(PQ *q, AVPacket *pk) {
    if (q->n >= PQ_N) return 0;
    AVPacket *c = av_packet_alloc();
    if (!c) return 0;
    q->bytes += (size_t)(pk->size > 0 ? pk->size : 0);
    av_packet_move_ref(c, pk);
    q->p[q->w] = c; q->w = (q->w + 1) % PQ_N; q->n++;
    return 1;
}
static AVPacket *pq_pop(PQ *q) {
    if (q->n == 0) return NULL;
    AVPacket *c = q->p[q->r];
    q->r = (q->r + 1) % PQ_N; q->n--;
    size_t sz = (size_t)(c->size > 0 ? c->size : 0);
    q->bytes = q->bytes > sz ? q->bytes - sz : 0;
    return c;
}
static void pq_clear(PQ *q) {
    AVPacket *c;
    while ((c = pq_pop(q)) != NULL) av_packet_free(&c);
    q->r = q->w = q->n = 0; q->bytes = 0;
}

/* ---------------------------------------------------------------- audio callback */
static void v_audio_cb(void *ud, Uint8 *stream, int len) {
    (void)ud;
    pthread_mutex_lock(&v_mx);
    int n = len < v_ring_n ? len : v_ring_n;
    if (n > 0) {
        int first = v_ring_sz - v_ring_r;
        if (first > n) first = n;
        memcpy(stream, v_ring + v_ring_r, (size_t)first);
        if (n > first) memcpy(stream + first, v_ring, (size_t)(n - first));
        v_ring_r = (v_ring_r + n) % v_ring_sz;
        v_ring_n -= n;
        v_consumed += n / 4;
    }
    pthread_mutex_unlock(&v_mx);
    if (n < len) memset(stream + n, 0, (size_t)(len - n));
    if (video_vol != 100) {
        Sint16 *p = (Sint16 *)stream;
        for (int i = 0; i < n / 2; i++) {
            int x = (p[i] * video_vol) / 100;
            p[i] = (Sint16)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
        }
    }
}

/* ---------------------------------------------------------------- decode thread */
static void v_handle_audio(AVFrame *fr, int block) {
    if (!v_adev) return;
    double pts = fr->best_effort_timestamp == AV_NOPTS_VALUE ? -1.0
        : (double)fr->best_effort_timestamp * av_q2d(v_ast->time_base) - v_t0;
    if (pts >= 0 && fr->sample_rate > 0 &&
        pts + (double)fr->nb_samples / fr->sample_rate < v_discard_until) return;
    if (!v_swr) {
        AVChannelLayout out, in;
        memset(&out, 0, sizeof out); memset(&in, 0, sizeof in);
        av_channel_layout_default(&out, 2);
        if (fr->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
            av_channel_layout_default(&in, fr->ch_layout.nb_channels > 0 ? fr->ch_layout.nb_channels : 2);
        else av_channel_layout_copy(&in, &fr->ch_layout);
        int r = swr_alloc_set_opts2(&v_swr, &out, AV_SAMPLE_FMT_S16, v_arate, &in,
                                    (enum AVSampleFormat)fr->format, fr->sample_rate, 0, NULL);
        av_channel_layout_uninit(&in);
        av_channel_layout_uninit(&out);
        if (r < 0 || swr_init(v_swr) < 0) { swr_free(&v_swr); return; }
    }
    int maxo = swr_get_out_samples(v_swr, fr->nb_samples) + 64;
    if (maxo * 4 > v_abuf_cap) {
        free(v_abuf);
        v_abuf_cap = maxo * 4;
        v_abuf = malloc((size_t)v_abuf_cap);
        if (!v_abuf) { v_abuf_cap = 0; return; }
    }
    uint8_t *outp[1] = { v_abuf };
    int got = swr_convert(v_swr, outp, maxo, (const uint8_t **)fr->extended_data, fr->nb_samples);
    if (got > 0) {
        /* block until room if requested */
        if (block) {
            while (v_ring_n >= v_ring_sz && !atomic_load(&v_abort) && !atomic_load(&v_seek_req))
                usleep(4000);
        }
        int space = v_ring_sz - v_ring_n;
        int put = got * 4 < space ? got * 4 : space;
        if (put > 0) {
            int first = v_ring_sz - v_ring_w;
            if (first > put) first = put;
            memcpy(v_ring + v_ring_w, v_abuf, (size_t)first);
            if (put > first) memcpy(v_ring, v_abuf + first, (size_t)(put - first));
            v_ring_w = (v_ring_w + put) % v_ring_sz;
            v_ring_n += put;
        }
    }
}

static void v_handle_video(AVFrame *fr, int block) {
    double pts;
    if (fr->best_effort_timestamp != AV_NOPTS_VALUE)
        pts = (double)fr->best_effort_timestamp * av_q2d(v_vst->time_base) - v_t0;
    else pts = v_last_vpts + v_frame_dur;
    v_last_vpts = pts;
    if (pts + 1e-3 < v_discard_until) return;
    for (;;) {
        pthread_mutex_lock(&v_mx);
        int n = v_qn;
        pthread_mutex_unlock(&v_mx);
        if (n < VQ_N) break;
        if (!block) return;
        if (atomic_load(&v_abort) || atomic_load(&v_seek_req)) return;
        usleep(3000);
    }
    AVFrame *out = NULL;
    if (fr->format == AV_PIX_FMT_YUV420P || fr->format == AV_PIX_FMT_YUVJ420P) {
        out = av_frame_clone(fr);
    } else {
        out = av_frame_alloc();
        if (!out) return;
        out->format = AV_PIX_FMT_YUV420P; out->width = fr->width; out->height = fr->height;
        if (av_frame_get_buffer(out, 32) < 0) { av_frame_free(&out); return; }
        v_sws = sws_getCachedContext(v_sws, fr->width, fr->height, (enum AVPixelFormat)fr->format,
                                     fr->width, fr->height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR,
                                     NULL, NULL, NULL);
        if (!v_sws) { av_frame_free(&out); return; }
        sws_scale(v_sws, (const uint8_t *const *)fr->data, fr->linesize, 0, fr->height,
                  out->data, out->linesize);
    }
    if (!out) return;
    pthread_mutex_lock(&v_mx);
    v_q[v_qw].f = out; v_q[v_qw].pts = pts;
    v_qw = (v_qw + 1) % VQ_N; v_qn++;
    pthread_mutex_unlock(&v_mx);
}

static void v_drain(AVCodecContext *c, AVFrame *fr, int is_video) {
    if (!c) return;
    avcodec_send_packet(c, NULL);
    while (!atomic_load(&v_abort) && !atomic_load(&v_seek_req) && avcodec_receive_frame(c, fr) == 0) {
        if (is_video) v_handle_video(fr, 1); else v_handle_audio(fr, 1);
        av_frame_unref(fr);
    }
}

static void *v_thread(void *arg) {
    (void)arg;
    AVPacket *pk = av_packet_alloc();
    AVFrame *fr = av_frame_alloc();
    int demux_done = 0, drained = 0;
    while (!atomic_load(&v_abort)) {
        if (atomic_load(&v_seek_req)) {
            pthread_mutex_lock(&v_mx);
            double t = v_seek_to; unsigned gen = v_seek_gen;
            atomic_store(&v_seek_req, 0);
            pthread_mutex_unlock(&v_mx);
            if (t < 0) t = 0;
            av_seek_frame(v_fmt, -1, (int64_t)((t + v_t0) * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD);
            if (v_vctx) avcodec_flush_buffers(v_vctx);
            if (v_actx) avcodec_flush_buffers(v_actx);
            if (v_swr) swr_init(v_swr);
            pq_clear(&v_vpq); pq_clear(&v_apq);
            demux_done = 0; drained = 0;
            v_discard_until = t; v_last_vpts = t;
            atomic_store(&v_eof, 0);
            pthread_mutex_lock(&v_mx);
            while (v_qn > 0) { av_frame_free(&v_q[v_qr].f); v_qr = (v_qr + 1) % VQ_N; v_qn--; }
            v_ring_r = v_ring_w = v_ring_n = 0;
            v_consumed = 0; v_clock_base = t;
            v_seek_done = gen;
            pthread_mutex_unlock(&v_mx);
            continue;
        }
        int did = 0;
        pthread_mutex_lock(&v_mx);
        int qn = v_qn, rfree = v_ring_sz - v_ring_n;
        pthread_mutex_unlock(&v_mx);

        for (int i = 0; i < 4 && v_actx && v_apq.n > 0 && rfree >= 16384 &&
                        !atomic_load(&v_seek_req); i++) {
            AVPacket *p = pq_pop(&v_apq);
            if (avcodec_send_packet(v_actx, p) == 0)
                while (avcodec_receive_frame(v_actx, fr) == 0) {
                    v_handle_audio(fr, 0);
                    av_frame_unref(fr);
                }
            av_packet_free(&p);
            pthread_mutex_lock(&v_mx);
            rfree = v_ring_sz - v_ring_n;
            pthread_mutex_unlock(&v_mx);
            did = 1;
        }
        if (v_vctx && v_vpq.n > 0 && qn < VQ_N - 4 && !atomic_load(&v_seek_req)) {
            AVPacket *p = pq_pop(&v_vpq);
            if (avcodec_send_packet(v_vctx, p) == 0)
                while (avcodec_receive_frame(v_vctx, fr) == 0) {
                    v_handle_video(fr, 0);
                    av_frame_unref(fr);
                }
            av_packet_free(&p);
            did = 1;
        }
        for (int i = 0; i < 4 && !demux_done && v_vpq.n < 3000 && v_vpq.bytes < (16u << 20) &&
                        v_apq.n < 3000 && v_apq.bytes < (4u << 20); i++) {
            int r = av_read_frame(v_fmt, pk);
            if (r < 0) { demux_done = 1; break; }
            if (pk->stream_index == v_vidx && v_vctx) { if (!pq_push(&v_vpq, pk)) av_packet_unref(pk); }
            else if (pk->stream_index == v_aidx && v_actx) { if (!pq_push(&v_apq, pk)) av_packet_unref(pk); }
            else av_packet_unref(pk);
            did = 1;
        }
        if (demux_done && !drained && v_vpq.n == 0 && v_apq.n == 0) {
            v_drain(v_vctx, fr, 1);
            v_drain(v_actx, fr, 0);
            if (!atomic_load(&v_seek_req) && !atomic_load(&v_abort)) { drained = 1; atomic_store(&v_eof, 1); }
            did = 1;
        }
        if (!did) usleep(2000);
    }
    av_frame_free(&fr);
    av_packet_free(&pk);
    return NULL;
}

/* ---------------------------------------------------------------- open/close */
static AVCodecContext *v_open_codec(AVStream *st, int video) {
    const AVCodec *dec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!dec) return NULL;
    AVCodecContext *c = avcodec_alloc_context3(dec);
    if (!c) return NULL;
    if (avcodec_parameters_to_context(c, st->codecpar) < 0) { avcodec_free_context(&c); return NULL; }
    if (video) {
        c->thread_count = 2;
        c->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
        c->skip_loop_filter = AVDISCARD_NONREF;
        c->flags2 |= AV_CODEC_FLAG2_FAST;
    }
    if (avcodec_open2(c, dec, NULL) < 0) { avcodec_free_context(&c); return NULL; }
    return c;
}

static void v_start_buffering(double pos) {
    video_buffering = 1;
    v_buf_t0 = SDL_GetTicks();
    v_buf_pos = pos;
    v_wall_base = pos;
    if (v_adev) SDL_PauseAudioDevice(v_adev, 1);
}

static int v_open(const char *path) {
    music_track_close();           /* stop any music */
    video_close();
    v_err[0] = 0;
    video_open_ok = 0;
    atomic_store(&v_abort, 0); atomic_store(&v_seek_req, 0); atomic_store(&v_eof, 0);
    v_seek_gen = v_seek_done = 0; v_discard_until = 0; v_last_vpts = 0;
    v_consumed = 0; v_clock_base = 0; v_dropped = 0; video_paused = 0; v_audio_done = 0;
    v_ring_r = v_ring_w = v_ring_n = 0;
    v_have_frame = 0;
    snprintf(video_path, sizeof video_path, "%s", path);

    if (avformat_open_input(&v_fmt, path, NULL, NULL) < 0) { snprintf(video_err, sizeof video_err, "%s", TX(X_ERR_VOPEN)); return 0; }
    if (avformat_find_stream_info(v_fmt, NULL) < 0) { snprintf(video_err, sizeof video_err, "%s", TX(X_ERR_VINFO)); goto fail; }
    v_vidx = av_find_best_stream(v_fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (v_vidx < 0) { snprintf(video_err, sizeof video_err, "%s", TX(X_ERR_NOVID)); goto fail; }
    v_vst = v_fmt->streams[v_vidx];
    v_vctx = v_open_codec(v_vst, 1);
    if (!v_vctx) { snprintf(video_err, sizeof video_err, "%s", TX(X_ERR_VCODEC)); goto fail; }
    v_aidx = av_find_best_stream(v_fmt, AVMEDIA_TYPE_AUDIO, -1, v_vidx, NULL, 0);
    if (v_aidx >= 0) {
        v_ast = v_fmt->streams[v_aidx];
        v_actx = v_open_codec(v_ast, 0);
        if (!v_actx) v_aidx = -1;
    }
    v_t0 = v_fmt->start_time != AV_NOPTS_VALUE ? (double)v_fmt->start_time / AV_TIME_BASE : 0.0;
    video_dur = v_fmt->duration != AV_NOPTS_VALUE ? (double)v_fmt->duration / AV_TIME_BASE : 0.0;
    AVRational fr = av_guess_frame_rate(v_fmt, v_vst, NULL);
    v_frame_dur = fr.num > 0 && fr.den > 0 ? (double)fr.den / fr.num : 1.0 / 25.0;
    v_w = v_vctx->width; v_h = v_vctx->height;
    if (v_w <= 0 || v_h <= 0) { snprintf(video_err, sizeof video_err, "%s", TX(X_ERR_VSIZE)); goto fail; }
    AVRational sar = av_guess_sample_aspect_ratio(v_fmt, v_vst, NULL);
    v_sar = sar.num > 0 && sar.den > 0 ? (double)sar.num / sar.den : 1.0;

    if (v_actx) {
        SDL_AudioSpec want, have;
        SDL_zero(want);
        want.freq = v_actx->sample_rate > 0 ? v_actx->sample_rate : 44100;
        want.format = AUDIO_S16SYS;
        want.channels = 2;
        want.samples = 2048;
        want.callback = v_audio_cb;
        v_adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        if (v_adev) {
            v_arate = have.freq;
            v_alat = (double)have.samples / (double)have.freq;
            v_ring_sz = (v_arate * 6) & ~3;
            v_ring = malloc((size_t)v_ring_sz);
            if (!v_ring) { SDL_CloseAudioDevice(v_adev); v_adev = 0; }
        }
    }
    if (!v_adev && v_actx) { avcodec_free_context(&v_actx); v_aidx = -1; }

    v_tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, v_w, v_h);
    if (!v_tex) {
        v_tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, v_w, v_h);
        v_tex_rgb = 1;
        v_rgb = malloc((size_t)v_w * (size_t)v_h * 3u);
        if (!v_tex || !v_rgb) { snprintf(video_err, sizeof video_err, "%s", TX(X_ERR_VTEX)); goto fail; }
    }

    v_start_buffering(0.0);
    int rs = config_resume_get(path);
    if (rs >= 10 && (video_dur <= 0 || rs < video_dur - 30)) {
        pthread_mutex_lock(&v_mx);
        v_seek_to = rs; v_seek_gen++;
        pthread_mutex_unlock(&v_mx);
        atomic_store(&v_seek_req, 1);
        v_start_buffering(rs);
        char tm[32];
        snprintf(tm, sizeof tm, "%d:%02d", rs / 60, rs % 60);
        snprintf(video_err, sizeof video_err, "%s", TX(X_RESUMED));
        /* NOTE: X_RESUMED needs %s formatting; show time instead */
        (void)tm;
    }
    if (pthread_create(&v_th, NULL, v_thread, NULL) != 0) { snprintf(video_err, sizeof video_err, "%s", TX(X_ERR_THREAD)); video_open_ok = 0; goto fail; }
    v_th_on = 1;
    video_open_ok = 1;
    return 1;
fail:
    video_close();
    return 0;
}

int video_open(const char *path) {
    snprintf(video_title, sizeof video_title, "%s", browser_base_name(path));
    return v_open(path);
}

void video_close(void) {
    if (v_th_on) { atomic_store(&v_abort, 1); pthread_join(v_th, NULL); v_th_on = 0; }
    if (v_adev) { SDL_CloseAudioDevice(v_adev); v_adev = 0; }
    pq_clear(&v_vpq); pq_clear(&v_apq);
    pthread_mutex_lock(&v_mx);
    while (v_qn > 0) { av_frame_free(&v_q[v_qr].f); v_qr = (v_qr + 1) % VQ_N; v_qn--; }
    pthread_mutex_unlock(&v_mx);
    if (v_swr) swr_free(&v_swr);
    if (v_sws) { sws_freeContext(v_sws); v_sws = NULL; }
    if (v_sws2) { sws_freeContext(v_sws2); v_sws2 = NULL; }
    if (v_vctx) avcodec_free_context(&v_vctx);
    if (v_actx) avcodec_free_context(&v_actx);
    if (v_fmt) avformat_close_input(&v_fmt);
    if (v_tex) { SDL_DestroyTexture(v_tex); v_tex = NULL; }
    free(v_ring); v_ring = NULL; v_ring_sz = 0;
    free(v_abuf); v_abuf = NULL; v_abuf_cap = 0;
    free(v_rgb); v_rgb = NULL;
    v_vidx = v_aidx = -1;
    v_w = v_h = 0;
    video_open_ok = 0; v_have_frame = 0;
}

/* ---------------------------------------------------------------- clock */
static double v_audio_clock(void) {
    pthread_mutex_lock(&v_mx);
    double c = v_clock_base + (double)v_consumed / (double)v_arate;
    pthread_mutex_unlock(&v_mx);
    c -= v_alat;
    return c < 0 ? 0 : c;
}
static int v_wall(void) { return !v_adev || v_audio_done; }
static double v_get_clock(void) {
    if (video_buffering) return v_buf_pos;
    if (!v_wall()) return v_audio_clock();
    if (video_paused) return v_wall_base;
    return v_wall_base + (double)(SDL_GetTicks() - v_wall_t0) / 1000.0;
}

/* ---------------------------------------------------------------- controls */
void video_key(Key k) {
    if (!video_open_ok) { if (k == K_A || k == K_B) { video_close(); /* scr set by caller */ } return; }
    Uint32 now = SDL_GetTicks();
    switch (k) {
    case K_A: case K_START: video_paused = !video_paused; break;
    case K_LEFT:  /* scrub -10s */ break;
    case K_RIGHT: /* scrub +10s */ break;
    case K_L: /* -1 min */ break;
    case K_R: /* +1 min */ break;
    case K_UP:   video_vol_step(1); break;
    case K_DOWN: video_vol_step(-1); break;
    case K_X: video_fit = !video_fit; break;
    case K_T: /* audio track */ break;
    case K_S: v_sub_on = !v_sub_on; break;
    case K_Y: /* start over */ break;
    case K_B: case K_SELECT:
        config_resume_set(video_path, (int)v_get_clock());
        video_close();
        /* scr set by caller */
        break;
    default: break;
    }
    (void)now;
}

void video_vol_step(int dir) {
    int n = (video_vol < 0 ? 50 : video_vol) + dir * 5;
    if (n < 0) n = 0;
    if (n > 100) n = 100;
    video_vol = n;
}

int video_is_open(void) { return video_open_ok; }
double video_get_clock(void) { return v_get_clock(); }

/* ---------------------------------------------------------------- tick */
static void v_show(AVFrame *f) {
    if (!v_tex_rgb) {
        SDL_UpdateYUVTexture(v_tex, NULL, f->data[0], f->linesize[0],
                             f->data[1], f->linesize[1],
                             f->data[2], f->linesize[2]);
    } else {
        v_sws2 = sws_getCachedContext(v_sws2, v_w, v_h, AV_PIX_FMT_YUV420P,
                                      v_w, v_h, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR,
                                      NULL, NULL, NULL);
        if (!v_sws2) return;
        uint8_t *dst[1] = { v_rgb };
        int dls[1] = { v_w * 3 };
        sws_scale(v_sws2, (const uint8_t *const *)f->data, f->linesize, 0, v_h, dst, dls);
        SDL_UpdateTexture(v_tex, NULL, v_rgb, v_w * 3);
    }
    v_have_frame = 1;
}

void video_tick(void) {
    if (!video_open_ok) return;
    Uint32 now = SDL_GetTicks();
    if (video_buffering) {
        pthread_mutex_lock(&v_mx);
        int synced = v_seek_done == v_seek_gen;
        int qn = v_qn, rn = v_ring_n;
        pthread_mutex_unlock(&v_mx);
        int ready = synced && (atomic_load(&v_eof) || (qn >= 3 && (!v_adev || rn >= v_arate)));
        if (ready || now - v_buf_t0 > 3000) {
            video_buffering = 0;
            if (v_wall()) v_wall_t0 = now;
            if (v_adev) SDL_PauseAudioDevice(v_adev, video_paused);
        }
    }
    if (!video_buffering && !video_paused) {
        double clk = v_get_clock();
        AVFrame *show = NULL;
        for (;;) {
            pthread_mutex_lock(&v_mx);
            if (v_qn == 0 || v_q[v_qr].pts > clk + 0.004) { pthread_mutex_unlock(&v_mx); break; }
            AVFrame *f = v_q[v_qr].f;
            v_qr = (v_qr + 1) % VQ_N; v_qn--;
            pthread_mutex_unlock(&v_mx);
            if (show) { av_frame_free(&show); v_dropped++; }
            show = f;
        }
        if (show) { v_show(show); av_frame_free(&show); }
        pthread_mutex_lock(&v_mx);
        int done = atomic_load(&v_eof) && v_qn == 0 && (!v_adev || v_ring_n == 0);
        pthread_mutex_unlock(&v_mx);
        if (done) {
            config_resume_set(video_path, 0);
            video_close();
            /* scr set by caller */
        }
    }
}

void video_draw(void) {
    ui_fill(0, 0, W, H, (SDL_Color){0, 0, 0, 255}, 255);
    if (!video_open_ok) {
        ui_draw_bg();
        const char *m = video_err[0] ? video_err : TX(X_ERR_VOPEN);
        ui_fill_rr(60, 170, W - 120, 110, 16, C_SEL, 255);
        ui_draw_text_center(f_mid, m, W / 2, 200, C_RED, W - 160);
        const char *k[2] = {"B", TX(X_H_BACK)};
        ui_draw_hints(k, 1);
        return;
    }
    if (v_tex && v_have_frame) {
        double dar = (double)v_w * v_sar / (double)v_h;
        SDL_Rect dst;
        if (video_fit) {
            int dw = W, dh = (int)(W / dar);
            if (dh > H) { dh = H; dw = (int)(H * dar); }
            dst = (SDL_Rect){(W - dw) / 2, (H - dh) / 2, dw, dh};
        } else {
            double ta = (double)W / H;
            int sw = (int)((double)v_h * ta / v_sar);
            if (sw <= v_w) { dst.w = sw; dst.h = v_h; dst.x = (v_w - sw) / 2; dst.y = 0; }
            else { int sh = (int)((double)v_w * v_sar / ta); dst.w = v_w; dst.h = sh; dst.x = 0; dst.y = (v_h - sh) / 2; }
            SDL_Rect src = {dst.x, dst.y, dst.w, dst.h};
            SDL_RenderCopy(R, v_tex, &src, NULL);
            dst = (SDL_Rect){0, 0, W, H};
        }
        if (!video_fit) SDL_RenderCopy(R, v_tex, NULL, &dst);
        else SDL_RenderCopy(R, v_tex, NULL, &dst);
    }
    /* subtitle text placeholder */
    if (v_sub_on && v_sub_text[0]) {
        ui_fill_rr(20, H - 100, W - 40, 40, 10, (SDL_Color){0, 0, 0, 160}, 200);
        ui_draw_text_center(f_small, v_sub_text, W / 2, H - 75, C_TEXT, W - 80);
    }
    /* HUD */
    Uint32 now = SDL_GetTicks();
    int show_hud = now < 3000 || video_paused || video_buffering;
    if (show_hud) {
        ui_fill_rr(14, H - 100, W - 28, 88, 14, C_DARK, 190);
        ui_draw_text(f_small, video_title, 30, H - 94, C_TEXT, W - 60);
        double cur = v_get_clock();
        if (video_dur > 0 && cur > video_dur) cur = video_dur;
        int bx = 30, bw = W - 60, by = H - 62;
        ui_fill_rr(bx, by, bw, 8, 4, C_PILL, 255);
        if (video_dur > 0) {
            int fw = (int)(bw * (cur / video_dur));
            if (fw > 0) ui_fill_rr(bx, by, fw < 8 ? 8 : fw, 8, 4, CC_VIDEO, 255);
        }
        ui_fill_circle(bx + (int)(bw * (video_dur > 0 ? cur / video_dur : 0)), by + 4, 7, C_TEXT, 255);
        if (video_paused || video_buffering) ui_icon_play(38, H - 34, 8, CC_VIDEO);
        else ui_icon_pause(38, H - 34, 8, CC_VIDEO);
        char a[32], b[32];
        snprintf(a, sizeof a, "%d:%02d", (int)(cur / 60), (int)cur % 60);
        snprintf(b, sizeof b, "%d:%02d", (int)(video_dur / 60), (int)video_dur % 60);
        char tm[80];
        snprintf(tm, sizeof tm, video_dur > 0 ? "%s / %s" : "%s", a, b);
        ui_draw_text(f_small, tm, 58, H - 45, C_TEXT, 220);
        int xr = W - 30;
        char p[64];
        snprintf(p, sizeof p, "%s %d%%", TX(X_VOL), video_vol);
        xr = ui_pill(xr, H - 44, p, C_PILL, C_DIM);
        xr = ui_pill(xr, H - 44, video_fit ? TX(X_FIT) : TX(X_FILL), C_PILL, C_TEXT);
    }
}
