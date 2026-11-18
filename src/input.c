/*
 * input.c - controller + keyboard input for mediaplay-next.
 */

#include "mediaplay.h"

#include <stdio.h>
#include <stdlib.h>

SDL_GameController *pad = NULL;

/* forward declared in player.c */
void handle_key(Key k);

/* ---------------------------------------------------------------- pad */
int input_pad_attached(void) { return pad != NULL; }

void input_open_pad(void) {
    if (pad) return;
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i)) { pad = SDL_GameControllerOpen(i); if (pad) break; }
}

/* ---------------------------------------------------------------- direction polling */
int input_read_dirs(void) {
    if (!pad) return 0;
    int m = 0;
    int ax = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
    int ay = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP))    m |= 1;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  m |= 2;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  m |= 4;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) m |= 8;
    if (abs(ax) > 16000 || abs(ay) > 16000) {
        if (abs(ax) >= abs(ay)) m |= ax < 0 ? 4 : 8;
        else m |= ay < 0 ? 1 : 2;
    }
    return m;
}

/* ---------------------------------------------------------------- event handling */
void input_handle_event(const SDL_Event *ev) {
    switch (ev->type) {
    case SDL_QUIT:
        running = 0;
        break;
    case SDL_CONTROLLERDEVICEADDED:
        input_open_pad();
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        if (pad && !SDL_GameControllerGetAttached(pad)) {
            SDL_GameControllerClose(pad);
            pad = NULL;
        }
        break;
    case SDL_CONTROLLERBUTTONDOWN:
        switch (ev->cbutton_button) {
        case SDL_CONTROLLER_BUTTON_A: handle_key(K_A); break;
        case SDL_CONTROLLER_BUTTON_B: handle_key(K_B); break;
        case SDL_CONTROLLER_BUTTON_X: handle_key(K_X); break;
        case SDL_CONTROLLER_BUTTON_Y: handle_key(K_Y); break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: handle_key(K_L); break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: handle_key(K_R); break;
        case SDL_CONTROLLER_BUTTON_START: handle_key(K_START); break;
        case SDL_CONTROLLER_BUTTON_BACK: handle_key(K_SELECT); break;
        default: break;
        }
        break;
    case SDL_CONTROLLERAXISMOTION:
        if (ev->caxis_axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT && ev->caxis_value > 16000) handle_key(K_T);
        if (ev->caxis_axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT && ev->caxis_value > 16000) handle_key(K_S);
        break;
    case SDL_KEYDOWN:
        switch (ev->key_keysym) {
        case SDLK_UP: handle_key(K_UP); break;
        case SDLK_DOWN: handle_key(K_DOWN); break;
        case SDLK_LEFT: handle_key(K_LEFT); break;
        case SDLK_RIGHT: handle_key(K_RIGHT); break;
        case SDLK_RETURN: handle_key(K_A); break;
        case SDLK_ESCAPE: handle_key(K_B); break;
        default: break;
        }
        break;
    default: break;
    }
}
