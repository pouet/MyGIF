#ifndef GIF_H
#define GIF_H

typedef struct GIF_Surface_s GIF_Surface;

GIF_Surface * GIF_LoadGIF(char * file);
SDL_Surface * GIF_GetNextFrame(GIF_Surface * gif);

Uint16 GIF_GetWidth(GIF_Surface *gif);
Uint16 GIF_GetHeight(GIF_Surface *gif);

#endif

