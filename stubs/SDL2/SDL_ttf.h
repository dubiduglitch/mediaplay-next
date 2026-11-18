#ifndef SDL_TTF_STUB_H
#define SDL_TTF_STUB_H
#include "SDL.h"
typedef int TTF_Font;
int TTF_Init(void); void TTF_Quit(void);
TTF_Font* TTF_OpenFont(const char*,int); void TTF_CloseFont(TTF_Font*);
int TTF_SizeUTF8(TTF_Font*,const char*,int*,int*);
SDL_Surface* TTF_RenderUTF8_Blended(TTF_Font*,const char*,SDL_Color);
#endif
