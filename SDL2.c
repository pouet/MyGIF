#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL.h>

#include "GIF.h"


enum {
  SDL_FLAGS = SDL_INIT_VIDEO,
  WIDTH = 800,
  HEIGHT = 600,
  BPP = 32,
  SFC_FLAGS = SDL_SWSURFACE
};



Sint32 init(void) {
  if (SDL_Init(SDL_FLAGS) < 0) {
    fprintf(stderr, "SDL_Init : %s\n", SDL_GetError());
    return -1;
  }
  atexit(SDL_Quit);
  
  if (SDL_SetVideoMode(WIDTH, HEIGHT, BPP, SFC_FLAGS) == NULL) {
    fprintf(stderr, "SDL_SetVideoMode : %s\n",
            SDL_GetError());
    return -1;
  }
  
  SDL_WM_SetCaption("GIF", NULL);
  
  return 0;
}

int main(int argc, char ** argv) {
  GIF_Surface * img;
  SDL_Surface * screen;
  SDL_Surface * tmp;
  SDL_Event e;
  Uint8 done;

  if (argc != 2) {
    fprintf(stderr, "%s requires one argument.\n", argv[0]);
    return EXIT_FAILURE;
  }
  
  if (init() < 0)
    return EXIT_FAILURE;

  img = GIF_LoadGIF(argv[1]);
  if (img == NULL) {
    fprintf(stderr, "Error: main: Open file failed !\n");
    return EXIT_FAILURE;
  }

	// Resize to match the size of the gif
  if (SDL_SetVideoMode(GIF_GetWidth(img), GIF_GetHeight(img), BPP, SFC_FLAGS) == NULL) {
    fprintf(stderr, "SDL_SetVideoMode : %s\n",
            SDL_GetError());
    return -1;
  }

  screen = SDL_GetVideoSurface();

  done = 0;
  while (!done) {
    if (SDL_QuitRequested())
        done = 1;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT ||
           (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
            done = 1;
    }
    SDL_FillRect(screen, NULL, 0xFF00FF00);
    tmp = GIF_GetNextFrame(img);
    SDL_BlitSurface(tmp, NULL, screen, NULL);
    SDL_Flip(screen);
    //SDL_Delay(1);
  }
  
  return EXIT_SUCCESS;
}
