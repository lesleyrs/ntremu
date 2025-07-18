#ifndef EMULATOR_H
#define EMULATOR_H

// #include <SDL2/SDL.h>

#include "emulator_state.h"
#include "gamecard.h"
#include "nds.h"
#include "types.h"

typedef struct SDL_Rect {
    int x, y;
    int w, h;
} SDL_Rect;

int emulator_init(int argc, char** argv);
void emulator_quit();

void emulator_reset();

void read_args(int argc, char** argv);
void hotkey_press(int key, int code);
void update_input_keyboard(NDS* nds, uint8_t *keys);
// void update_input_controller(NDS* nds, SDL_GameController* controller);
void update_input_touch(NDS* nds, SDL_Rect* ts_bounds);
bool onmousemove(void *user_data, int button, int x, int y);
bool onmousedown(void *user_data, int button, int x, int y);
bool onmouseup(void *user_data, int button, int x, int y);
void update_input_freecam(uint8_t *keys);

#endif
