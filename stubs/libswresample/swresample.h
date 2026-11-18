#ifndef SWRESAMPLE_STUB_H
#define SWRESAMPLE_STUB_H
typedef struct SwrContext SwrContext;
SwrContext* swr_alloc_set_opts2(SwrContext**, const void*, const void*, int, const void*, int, int, int, void*);
int swr_init(SwrContext*); int swr_get_out_samples(SwrContext*, int);
int swr_convert(SwrContext*, uint8_t**, int, const uint8_t**, int);
void swr_free(SwrContext**);
#endif
