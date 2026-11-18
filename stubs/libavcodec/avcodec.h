#ifndef AVCODEC_STUB_H
#define AVCODEC_STUB_H
#include <stdint.h>
typedef int AVCodecID; typedef int AVSampleFormat; typedef int AVPixelFormat;
typedef struct AVCodec AVCodec; typedef struct AVCodecContext AVCodecContext;
typedef struct AVFrame AVFrame; typedef struct AVPacket AVPacket;
typedef struct AVStream AVStream;
const AVCodec* avcodec_find_decoder(AVCodecID); AVCodecContext* avcodec_alloc_context3(const AVCodec*);
int avcodec_parameters_to_context(AVCodecContext*, void*); void avcodec_free_context(AVCodecContext**);
int avcodec_open2(AVCodecContext*, const AVCodec*, void**);
int avcodec_send_packet(AVCodecContext*, AVPacket*); int avcodec_receive_frame(AVCodecContext*, AVFrame*);
void avcodec_flush_buffers(AVCodecContext*); AVFrame* av_frame_alloc(void); AVFrame* av_frame_clone(const AVFrame*);
void av_frame_free(AVFrame**); void av_frame_unref(AVFrame*);
int av_frame_get_buffer(AVFrame*, int);
enum { AV_NOPTS_VALUE = (int64_t)0x8000000000000000 };
enum { AV_CODEC_FLAG2_FAST=0x10000000 };
enum { AVDISCARD_NONREF=1 };
enum { FF_THREAD_FRAME=1, FF_THREAD_SLICE=2 };
enum { AV_SAMPLE_FMT_S16=0, AV_PIX_FMT_YUV420P=0, AV_PIX_FMT_YUVJ420P=1, AV_PIX_FMT_RGB24=2 };
enum { SWS_FAST_BILINEAR=1 };
#endif
