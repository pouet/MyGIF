#ifndef GIF_STRUCT_H
#define GIF_STRUCT_H

#include <SDL.h>

enum {
  GIF_87A = 1,
  GIF_89A = 2,
  GIF_UKNOW = 3
};

typedef SDL_Color GIF_Color;

typedef struct {
  Uint16 imgLftPos;
  Uint16 imgTopPos;
  Uint16 imgWidth;
  Uint16 imgHeight;
  GIF_Color * lcolTable;
  Uint32 interlace    : 1;
  Uint16 * data;
  
  Uint32 dispMeth     : 3;
  Uint32 userInput    : 1;
  Uint32 transpColor  : 1;
  Uint16 delay;
  Uint8 transpColorIdx;
} GIF_Image;

typedef struct {
  Uint8 version;
  
  Uint16 w;
  Uint16 h;
  Uint8 bckColIndex;
  GIF_Color * gcolTable;
  GIF_Image * img;
  
  Uint32 i;
  
} GIF_Raw;


Uint8 * pdata;

#endif
