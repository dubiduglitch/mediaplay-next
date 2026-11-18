#ifndef SWSCALE_STUB_H
#define SWSCALE_STUB_H
typedef struct SwsContext SwsContext;
SwsContext* sws_getCachedContext(SwsContext*, int, int, int, int, int, int, int, void*, void*, void*);
int sws_scale(SwsContext*, const uint8_t* const*, const int*, int, int, uint8_t**, const int*);
void sws_freeContext(SwsContext*);
#endif
