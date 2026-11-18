#ifndef SDL_STUB_H
#define SDL_STUB_H
#include <stdint.h>
typedef uint32_t Uint32; typedef int32_t Sint32;
typedef uint16_t Uint16; typedef int16_t Sint16;
typedef uint8_t  Uint8;  typedef int8_t  Sint8;
typedef int SDL_Window; typedef int SDL_Renderer; typedef int SDL_Texture;
typedef struct SDL_Surface { int w, h; Uint32 format; void* pixels; int pitch; } SDL_Surface; typedef int SDL_AudioDeviceID;
typedef int SDL_GameController; typedef int SDL_Joystick;
enum { SDL_FALSE=0, SDL_TRUE=1 };
enum { SDL_INIT_VIDEO=0x20, SDL_INIT_AUDIO=0x10, SDL_INIT_GAMECONTROLLER=0x200 };
enum { SDL_WINDOWPOS_UNDEFINED=0 };
enum { SDL_RENDERER_ACCELERATED=1, SDL_RENDERER_PRESENTVSYNC=4, SDL_RENDERER_SOFTWARE=2 };
enum { SDL_HINT_RENDER_SCALE_QUALITY=1, SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS=2, SDL_DISABLE=0 };
enum { SDL_PIXELFORMAT_ARGB8888=0, SDL_PIXELFORMAT_IYUV=1, SDL_PIXELFORMAT_RGB24=2 };
enum { SDL_TEXTUREACCESS_STATIC=0, SDL_TEXTUREACCESS_STREAMING=1 };
enum { SDL_BLENDMODE_NONE=0, SDL_BLENDMODE_BLEND=1 };
enum {
  SDL_CONTROLLER_BUTTON_A=0, SDL_CONTROLLER_BUTTON_B=1, SDL_CONTROLLER_BUTTON_X=2,
  SDL_CONTROLLER_BUTTON_Y=3, SDL_CONTROLLER_BUTTON_LEFTSHOULDER=4,
  SDL_CONTROLLER_BUTTON_RIGHTSHOULDER=5, SDL_CONTROLLER_BUTTON_BACK=6,
  SDL_CONTROLLER_BUTTON_START=7,
  SDL_CONTROLLER_BUTTON_DPAD_UP=11, SDL_CONTROLLER_BUTTON_DPAD_DOWN=12,
  SDL_CONTROLLER_BUTTON_DPAD_LEFT=13, SDL_CONTROLLER_BUTTON_DPAD_RIGHT=14,
  SDL_CONTROLLER_AXIS_LEFTX=0, SDL_CONTROLLER_AXIS_LEFTY=1,
  SDL_CONTROLLER_AXIS_TRIGGERLEFT=4, SDL_CONTROLLER_AXIS_TRIGGERRIGHT=5,
  SDL_QUIT=0x100,
  SDL_CONTROLLERDEVICEADDED=0x401, SDL_CONTROLLERDEVICEREMOVED=0x402,
  SDL_CONTROLLERBUTTONDOWN=0x404, SDL_CONTROLLERAXISMOTION=0x403,
  SDL_KEYDOWN=0x300,
  SDLK_UP=1, SDLK_DOWN=2, SDLK_LEFT=3, SDLK_RIGHT=4, SDLK_RETURN=13, SDLK_ESCAPE=27
};
typedef struct { int type; int cbutton_button; int caxis_axis; int caxis_value; int key_keysym; } SDL_Event;
typedef struct { Uint8 r, g, b, a; } SDL_Color;
typedef struct { int x, y, w, h; } SDL_Rect;
typedef struct { int freq; int format; int channels; Uint16 samples; void (*callback)(void*,Uint8*,int); } SDL_AudioSpec;
int SDL_Init(unsigned); void SDL_Quit(void); int SDL_SetHint(const char*,const char*);
SDL_Window* SDL_CreateWindow(const char*,int,int,int,int,unsigned);
SDL_Renderer* SDL_CreateRenderer(SDL_Window*,int,unsigned);
int SDL_RenderSetLogicalSize(SDL_Renderer*,int,int);
void SDL_ShowCursor(int); int SDL_NumJoysticks(void); int SDL_IsGameController(int);
SDL_GameController* SDL_GameControllerOpen(int);
int SDL_GameControllerGetAttached(SDL_GameController*); void SDL_GameControllerClose(SDL_GameController*);
int SDL_GameControllerGetButton(SDL_GameController*,int); int SDL_GameControllerGetAxis(SDL_GameController*,int);
int SDL_PollEvent(SDL_Event*); int SDL_WaitEventTimeout(SDL_Event*,int);
int SDL_RenderClear(SDL_Renderer*); int SDL_RenderPresent(SDL_Renderer*);
int SDL_SetRenderDrawBlendMode(SDL_Renderer*,int); int SDL_SetRenderDrawColor(SDL_Renderer*,Uint8,Uint8,Uint8,Uint8);
int SDL_RenderFillRect(SDL_Renderer*,const SDL_Rect*);
int SDL_RenderCopy(SDL_Renderer*,SDL_Texture*,const SDL_Rect*,const SDL_Rect*);
SDL_Texture* SDL_CreateTexture(SDL_Renderer*,unsigned,int,int,int);
int SDL_UpdateTexture(SDL_Texture*,const SDL_Rect*,const void*,int);
int SDL_UpdateYUVTexture(SDL_Texture*,const SDL_Rect*,const Uint8*,int,const Uint8*,int,const Uint8*,int);
int SDL_DestroyTexture(SDL_Texture*); int SDL_DestroyRenderer(SDL_Renderer*); int SDL_DestroyWindow(SDL_Window*);
SDL_AudioDeviceID SDL_OpenAudioDevice(const char*,int,const SDL_AudioSpec*,SDL_AudioSpec*,int);
int SDL_PauseAudioDevice(SDL_AudioDeviceID,int); int SDL_CloseAudioDevice(SDL_AudioDeviceID);
Uint32 SDL_GetTicks(void); const char* SDL_GetError(void);
SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer*, SDL_Surface*);
void SDL_FreeSurface(SDL_Surface*);
int SDL_GetRendererInfo(SDL_Renderer*, void*);
#endif
