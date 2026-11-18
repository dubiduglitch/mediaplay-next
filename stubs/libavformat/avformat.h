#ifndef AVFORMAT_STUB_H
#define AVFORMAT_STUB_H
#include <stdint.h>
typedef struct AVFormatContext AVFormatContext;
typedef struct AVInputFormat AVInputFormat;
typedef struct AVDictionary AVDictionary;
int avformat_open_input(AVFormatContext**, const char*, const AVInputFormat*, AVDictionary**);
int avformat_find_stream_info(AVFormatContext*, void**);
int av_find_best_stream(AVFormatContext*, int, int, int, void**, int);
int av_guess_frame_rate(AVFormatContext*, AVStream*, void*);
void avformat_close_input(AVFormatContext**);
enum { AVMEDIA_TYPE_VIDEO=0, AVMEDIA_TYPE_AUDIO=1 };
enum { AVSEEK_FLAG_BACKWARD=1 };
int av_seek_frame(AVFormatContext*, int, int64_t, int);
enum { AV_TIME_BASE = 1000000 };
typedef int64_t int64_t;
#endif
