#pragma once

#include <SDL2/SDL.h>

const int CELL_SIZE = 24;
const int CELL_COUNT = 30;

const int SCREEN_WIDTH = CELL_SIZE * CELL_COUNT;
const int SCREEN_HEIGHT = CELL_SIZE * CELL_COUNT;

int startSDL(SDL_Window *window, SDL_Renderer *renderer);