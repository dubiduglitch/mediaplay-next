#ifndef SDL_MIXER_STUB_H
#define SDL_MIXER_STUB_H
#include "SDL.h"
typedef int Mix_Music;
enum { MIX_DEFAULT_FORMAT=0, MIX_MAX_VOLUME=128 };
int Mix_OpenAudio(int,int,int,int); void Mix_CloseAudio(void);
int Mix_VolumeMusic(int);
Mix_Music* Mix_LoadMUS(const char*); void Mix_FreeMusic(Mix_Music*);
int Mix_PlayMusic(Mix_Music*,int); int Mix_PauseMusic(int); int Mix_HaltMusic(void);
int Mix_PlayingMusic(void);
const char* Mix_GetError(void);
#endif
