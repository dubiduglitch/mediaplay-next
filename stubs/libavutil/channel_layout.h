#ifndef AVUTIL_CHANNEL_LAYOUT_STUB_H
#define AVUTIL_CHANNEL_LAYOUT_STUB_H
typedef struct AVChannelLayout { int order; int nb_channels; uint64_t channel_mask; } AVChannelLayout;
void av_channel_layout_default(AVChannelLayout*, int); void av_channel_layout_copy(AVChannelLayout*, const AVChannelLayout*);
void av_channel_layout_uninit(AVChannelLayout*);
enum { AV_CHANNEL_ORDER_UNSPEC = -1 };
#endif
