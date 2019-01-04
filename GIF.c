#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL.h>

#include "GIF.h"
#include "GIF_Struct.h"
#include "GIF_LZW.h"

enum {
  NDEFCOLTAB = 256,
  NBITDEFCOLTAB = 7
};

GIF_Color defaultColTable[NDEFCOLTAB] = {
  { 0x00, 0x00, 0x00, 0x00 },
  { 0xFF, 0xFF, 0xFF, 0x00 }
};



struct GIF_Surface_s {
  SDL_Surface ** images;
  Uint16 * delays;
  Uint32 nimg;
  Uint32 i;
  
  Uint32 tnxt;

	Uint16 w;
	Uint16 h;
};




Uint16 GIF_GetWidth(GIF_Surface *gif) {
	return gif->w;
}

Uint16 GIF_GetHeight(GIF_Surface *gif) {
	return gif->h;
}


/* Get nb bytes in s and return it byte ordered */
unsigned GIF_GetInt(Uint8 * s, int nb) {
  unsigned n = 0;
  int i;
  
  for (i = 0; i < nb; i++) {
    n <<= 8;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    n |= s[i];
#elif SDL_BYTEORDER == SDL_LIL_ENDIAN
    n |= s[nb - i - 1];
#endif
  }
  
  return n;
}




Sint8 GIF_GetColorTable(GIF_Color ** cols, unsigned n) {
  unsigned sz;
  unsigned i;
  
  /* 3 x 2^(Size of [Global/Local] Color Table + 1) */
  sz = 1 << (n + 1);
  *cols = malloc(sz * sizeof **cols);
  if (*cols == NULL)
    return -1;
  
  for (i = 0; i < sz; i++) {
    (*cols)[i].r = *pdata++;
    (*cols)[i].g = *pdata++;
    (*cols)[i].b = *pdata++;
  }
  
  return 0;
}


/* Header - 6 bytes :
 * -> 3 bytes : signature - "GIF"
 * -> 3 bytes : version - 87A / 89A
 */
Uint8 GIF_GetHeader(void) {
  Uint8 vers;
  
  if (strncmp((const char*)pdata, "GIF", 3))
    return GIF_UKNOW;
  
  pdata += 3;
  
  if (strncmp((const char*)pdata, "87a", 3) == 0)
    vers = GIF_87A;
  else if (strncmp((const char*)pdata, "89a", 3) == 0)
    vers = GIF_89A;
  else
    vers = GIF_UKNOW;
  
  pdata += 3;
  
  return vers;
}

/* Logical Screen Descriptor - 6 bytes :
 * -> 2 bytes : Logical screen width
 * -> 2 bytes : Logical screen height
 * -> 1 byte  : - 1 bit  : global color table flag
 *              - 3 bits : color resolution
 *              - 1 bit  : sort flag
 *              - 3 bits : size of global color table
 * -> 1 byte  : background color index
 * -> 1 byte  : pixel aspect ratio
 */

Sint8 GIF_GetLogScrDescriptor(GIF_Raw * gif) {
  Uint8 gcolTable;
  Uint8 szgcolTable;
  
  gif->w = GIF_GetInt(pdata, 2);
  pdata += 2;
  
  gif->h = GIF_GetInt(pdata, 2);
  pdata += 2;
  
  gcolTable = (*pdata) >> 7;
  szgcolTable = (*pdata) & 0x7;
  pdata++;
  
  gif->bckColIndex = *pdata;
  pdata++;
  
  /* Pixel aspect ratio, skipping... */
  pdata++;
  
  if (gcolTable) {
    if (GIF_GetColorTable(&gif->gcolTable, szgcolTable) < 0)
      return -1;
  }
  else {
    gif->gcolTable = defaultColTable;
  }
  
  return 0;
}

/* Image Descriptor - 9 bytes :
 * -> 1 byte  : Image separator - fixed value 0x2C
 * -> 2 bytes : Image left position
 * -> 2 bytes : Image top position
 * -> 2 bytes : Image width
 * -> 2 bytes : Image height
 * -> 1 byte  : - 1 bit  : local color table flag
 *              - 1 bit  : interlace flag
 *              - 1 bit  : sort flag
 *              - 2 bits : reserved
 *              - 3 bits : size of local color table
 */

Sint8 GIF_GetImgDescriptor(GIF_Raw * gif) {
  GIF_Image * img = &gif->img[gif->i];
  Uint8 lcolTable;
  Uint8 szlcolTable;
  
  img->imgLftPos = GIF_GetInt(pdata, 2);
  pdata += 2;
  
  img->imgTopPos = GIF_GetInt(pdata, 2);
  pdata += 2;
  
  img->imgWidth = GIF_GetInt(pdata, 2);
  pdata += 2;
  
  img->imgHeight = GIF_GetInt(pdata, 2);
  pdata += 2;
  
  lcolTable = ((*pdata) >> 7) & 0x1;
  img->interlace = ((*pdata) >> 6) & 0x1;
  szlcolTable = (*pdata) & 0x7;
  pdata++;
  
  if (lcolTable) {
    if (GIF_GetColorTable(&img->lcolTable, szlcolTable) < 0)
      return -1;
  }
  else {
    img->lcolTable = gif->gcolTable;
  }
  
  return 0;
}



/* Extensions */
/* #pragma mark Extensions */

/* Graphic Control Extension - 7 bytes :
 * -> 1 byte  : Extension introducer - fixed value 0x21
 * -> 1 byte  : Graphic control label - fixed value 0xF9
 * -> 1 byte  : Block size - fixed value 4
 * -> 1 byte  : - 3 bits : reserved
 *              - 3 bits : disposal method
 *              - 1 bit  : user input flag
 *              - 1 bit  : transparent color flag
 * -> 2 bytes : Delay time
 * -> 1 byte  : Transparent color index
 * -> 1 byte  : block terminator - fixed value 0x00
 */

Sint8 GIF_GetGraphCtrlExt(GIF_Raw * gif) {
  if (*pdata++ != 4)
    return -1;
  
  gif->img[gif->i].dispMeth = ((*pdata) >> 2) & 0x7;
  gif->img[gif->i].userInput = ((*pdata) >> 1) & 0x1;
  gif->img[gif->i].transpColor = (*pdata) & 0x1;
  pdata++;
  
  gif->img[gif->i].delay = GIF_GetInt(pdata, 2);
  pdata += 2;
  
  gif->img[gif->i].transpColorIdx = *pdata;
  pdata++;
  
  /* Skip the block terminator */
  pdata++;
  
  return 0;
}

/* Comment Extension - 3 bytes + N bytes :
 * -> 1 byte  : Extension introducer - fixed value 0x21
 * -> 1 byte  : Comment label - fixed value 0xFE
 * -> 8 bytes : Application identifier
 * -> 3 bytes : App. authentification code
 *
 * -> Data sub-blocks
 *
 * -> 1 byte  : block terminator - fixed value 0x00
 */ 

Sint8 GIF_GetCommExt(void) {
  Uint32 i, tmp;
  Uint8 s[256];
  
  printf("Comment ext. : ");
  
  while (*pdata != 0) {
    tmp = *pdata;
    pdata++;
    
    for (i = 0; i < tmp; i++)
      s[i] = pdata[i];
    s[i] = '\0';
    
    printf("%s", s);
    
    pdata += tmp;
  }
  
  puts("");
  
  /* Skip the block terminator */
  pdata++;
  
  return 0;
}

/* Application Extension - 14 bytes + N bytes :
 * -> 1 byte  : Extension introducer - fixed value 0x21
 * -> 1 byte  : Comment label - fixed value 0xFF
 * -> 1 byte  : Block size - fixed value 11
 
 * -> Data sub-blocks
 *
 * -> 1 byte  : block terminator - fixed value 0x00
 */

Sint8 GIF_GetAppExt(void) {
  Uint32 i, tmp;
  Uint8 s[256];
  
  if (*pdata++ != 11)
    return -1;
  
  for (i = 0; i < 8; i++)
    s[i] = pdata[i];
  s[i] = '\0';
  pdata += 8;
  
  printf("App. identifier : %s\n", s);
  
  for (i = 0; i < 3; i++)
    s[i] = pdata[i];
  s[i] = '\0';
  pdata += 3;
  
  printf("App. auth. code : %s\n", s);
  printf("App. data : ");
  
  while (*pdata != 0) {
    tmp = *pdata;
    pdata++;
    
    for (i = 0; i < tmp; i++)
      s[i] = pdata[i];
    s[i] = '\0';
    
    printf("%s", s);
    
    pdata += tmp;
  }
  
  puts("");
  
  /* Skip the block terminator */
  pdata++;
  
  return 0;
}

/* An extension begin with a 0x21 byte */

Sint8 GIF_GetExtension(GIF_Raw * gif) {
  
  switch (*pdata++) {
    case 0x01:
      /* TODO: 25. Plain Text Extension. */
      break;
      
    case 0xF9:
      return GIF_GetGraphCtrlExt(gif);
      break;
      
    case 0xFE:
      return GIF_GetCommExt();
      break;
      
    case 0xFF:
      return GIF_GetAppExt();
      break;
      
    default:
      fprintf(stderr, "Unknown extension.\n");
      return -1;
      break;
  }
  
  return 0;
}

/* #pragma mark Images */

/*
 * Set the pixel at (x, y) to the given value
 * NOTE: The surface must be locked before calling this!
 */
void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
  int bpp = surface->format->BytesPerPixel;
  /* Here p is the address to the pixel we want to set */
  Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
  
  switch(bpp) {
    case 1:
      *p = pixel;
      break;
      
    case 2:
      *(Uint16 *)p = pixel;
      break;
      
    case 3:
      if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
        p[0] = (pixel >> 16) & 0xff;
        p[1] = (pixel >> 8) & 0xff;
        p[2] = pixel & 0xff;
      } else {
        p[0] = pixel & 0xff;
        p[1] = (pixel >> 8) & 0xff;
        p[2] = (pixel >> 16) & 0xff;
      }
      break;
      
    case 4:
      *(Uint32 *)p = pixel;
      break;
  }
}

/*
 * Return the pixel value at (x, y)
 * NOTE: The surface must be locked before calling this!
 */
Uint32 getpixel(SDL_Surface *surface, int x, int y)
{
  int bpp = surface->format->BytesPerPixel;
  /* Here p is the address to the pixel we want to retrieve */
  Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
  
  switch(bpp) {
    case 1:
      return *p;
      
    case 2:
      return *(Uint16 *)p;
      
    case 3:
      if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
        return p[0] << 16 | p[1] << 8 | p[2];
      else
        return p[0] | p[1] << 8 | p[2] << 16;
      
    case 4:
      return *(Uint32 *)p;
      
    default:
      return 0;       /* shouldn't happen, but avoid warnings */
  }
}


void GIF_InitImage(GIF_Raw * gif, GIF_Image * img) {
  img->delay = 0;
  img->dispMeth = 0;
  img->imgHeight = gif->h;
  img->imgWidth = gif->w;
  img->imgLftPos = 0;
  img->imgTopPos = 0;
  img->interlace = 0;
  img->transpColor = 0;
  img->userInput = 0;
  img->lcolTable = gif->gcolTable;
}

/* Get 1 image */

Sint8 GIF_GetImage(GIF_Raw * gif) {
  GIF_Image * img;
  
  img = realloc(gif->img, (gif->i + 1) * sizeof *gif->img);
  if (img == NULL)
    return -1;
  gif->img = img;
  
  img = &gif->img[gif->i];
  
  GIF_InitImage(gif, img);
  
  while (1) {
    
    switch (*pdata++) {
        /* Extensions */
      case 0x21:
        if (GIF_GetExtension(gif) < 0)
          return -1;
        break;
        
        /* Image */
      case 0x2C:
        if (GIF_GetImgDescriptor(gif) < 0)
          return -1;
        
        if (GIF_LZW_GetData(img) < 0)
          return -1;
        
        return 0;
        break;
        
        /* Trailer */
      case 0x3B:
        return 1;
        break;
      
      default:
        fprintf(stderr, "GIF_GetImage: Unknown code.\n");
        return -1;
        break;
    }
  }
  
  
  return 0;
}

/* Get all the images */

Sint8 GIF_GetImages(GIF_Raw * gif) {
  Sint8 tmp;
  
  gif->img = NULL;
  gif->i = 0;
  
  while (1) {
    tmp = GIF_GetImage(gif);
    
    if (tmp < 0) {
      fprintf(stderr, "GIF_GetImages: Frame error.\n");
      return -1;
    }
    else if (tmp == 1)
      break;
    
    gif->i++;
  }
  
  return 0;
}

SDL_Surface * GIF_CreateRGBSurface(Uint16 w, Uint16 h) {
  SDL_Surface * tmp;
  SDL_Surface * sfc;
  
  tmp = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, 0, 0, 0, 0);
  if (tmp == NULL)
    return NULL;
  
  sfc = SDL_DisplayFormat(tmp);
  SDL_FreeSurface(tmp);
  
  return sfc;
}

int GIF_BlitDispMethod1(SDL_Surface * dst, GIF_Image * img) {
  Uint32 x, y;
  Uint32 k, l;
  Uint32 h, w;
  Uint32 tmp;
  Uint32 col;
  
  h = img->imgHeight;
  w = img->imgWidth;
  
  if (SDL_MUSTLOCK(dst))
    SDL_LockSurface(dst);
  
  for (k = 0, y = img->imgTopPos; k < h; y++, k++) {
    for (l = 0, x = img->imgLftPos; l < w; x++, l++) {
      tmp = (k * w) + l;
            
      if (!img->transpColor ||
          (img->transpColor && img->data[tmp] != img->transpColorIdx)) {
        col = SDL_MapRGB(dst->format,
                         img->lcolTable[img->data[tmp]].r,
                         img->lcolTable[img->data[tmp]].g,
                         img->lcolTable[img->data[tmp]].b);
        
        putpixel(dst, x, y, col);
      }
    }
  }
  
  if (SDL_MUSTLOCK(dst))
    SDL_UnlockSurface(dst);
  
  return 0;
}

int GIF_BlitDispMethod2(SDL_Surface * dst, SDL_Rect * r, Uint32 color) {
  Uint32 x, y;
  Uint32 k, l;
  
  k = r->y + r->h;
  l = r->x + r->w;
  
  if (SDL_MUSTLOCK(dst))
    SDL_LockSurface(dst);
  
  for (y = r->y; y < k; y++) {
    for (x = r->x; x < l; x++) {
      putpixel(dst, x, y, color);
    }
  }
  
  if (SDL_MUSTLOCK(dst))
    SDL_UnlockSurface(dst);
  
  return 0;
}

int GIF_BlitDispMethod3(SDL_Surface * src, SDL_Surface * dst, SDL_Rect * r) {
  Uint32 x, y;
  Uint32 k, l;
  
  k = r->y + r->h;
  l = r->x + r->w;
  
  if (SDL_MUSTLOCK(src))
    SDL_LockSurface(src);
  if (SDL_MUSTLOCK(dst))
    SDL_LockSurface(dst);
  
  for (y = r->y; y < k; y++) {
    for (x = r->x; x < l; x++) {
      putpixel(dst, x, y, getpixel(src, x, y));
    }
  }
  
  if (SDL_MUSTLOCK(src))
    SDL_UnlockSurface(src);
  if (SDL_MUSTLOCK(dst))
    SDL_UnlockSurface(dst);
  
  return 0;
}

Sint8 GIF_RenderInterlace(GIF_Image * img, SDL_Surface * dst) {
  Uint8 start[4] = { 0, 4, 2, 1 };
  Uint8 off[4] = { 8, 8, 4, 2 };
  Uint32 i, j, k, l;
  Uint32 tmp;
  Uint32 col;
  
  if (SDL_MUSTLOCK(dst))
    SDL_LockSurface(dst);
  
  l = 0;
  for (k = 0; k < 4; k++) {
    for (i = start[k]; i < img->imgHeight; i += off[k]) {
      for (j = 0; j < img->imgWidth; j++) {
        tmp = (l * img->imgWidth) + j;
        
        col = SDL_MapRGB(dst->format,
                         img->lcolTable[img->data[tmp]].r,
                         img->lcolTable[img->data[tmp]].g,
                         img->lcolTable[img->data[tmp]].b);
        
        putpixel(dst, j, i, col);
      }
      l++;
    }
  }
  
  if (SDL_MUSTLOCK(dst))
    SDL_UnlockSurface(dst);
  
  return 0;
}

/* Put all the raw images in an array of SDL_Surface */

Sint8 GIF_RenderFrames(GIF_Raw * raw, GIF_Surface * gif) {
  SDL_Surface * sfc;
  SDL_Rect r;
  Uint32 col;
  Uint32 i, j;
  Uint32 alpha;
  
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
  alpha = 0xFF000000;
#elif SDL_BYTEORDER == SDL_LIL_ENDIAN
  alpha = 0x000000FF;
#endif
  
  sfc = GIF_CreateRGBSurface(raw->w, raw->h);
  if (sfc == NULL)
    return -1;
  
  SDL_FillRect(sfc, NULL, alpha);
  SDL_SetColorKey(sfc, SDL_SRCCOLORKEY, alpha);
  
  for (i = 0; i < gif->nimg; i++) {
    if (raw->img[i].interlace)
      GIF_RenderInterlace(&raw->img[i], gif->images[i]);
    else
      GIF_BlitDispMethod1(sfc, &raw->img[i]);
    
    SDL_BlitSurface(sfc, NULL, gif->images[i], NULL);
    
//    {
//      SDL_Surface * screen = SDL_GetVideoSurface();
//      
//      SDL_FillRect(screen, NULL, 0xFF00FF00);
//      SDL_BlitSurface(gif->images[i], NULL, screen, NULL);
//      SDL_Flip(screen);
//      SDL_Delay(1000);
//      
//      
//    }
    
    switch (raw->img[i].dispMeth) {
      case 0:
      case 1:
      default:
        break;
        
      case 2:
        r.x = raw->img[i].imgLftPos;
        r.y = raw->img[i].imgTopPos;
        r.w = raw->img[i].imgWidth;
        r.h = raw->img[i].imgHeight;
        col = alpha;
        
        GIF_BlitDispMethod2(sfc, &r, col);
        break;
        
      case 3:
        r.x = raw->img[i].imgLftPos;
        r.y = raw->img[i].imgTopPos;
        r.w = raw->img[i].imgWidth;
        r.h = raw->img[i].imgHeight;
        
        if (i == 0)
          break;
        
        j = i;
        while (j > 0 && raw->img[j - 1].dispMeth == 3)
          j--;
        
        GIF_BlitDispMethod3(gif->images[j - 1], sfc, &r);
        break;
    }
    
  }
  
  SDL_FreeSurface(sfc);
  
  return 0;
}

Sint8 GIF_InitFrames(GIF_Raw * raw, GIF_Surface * gif) {
  Uint32 i;
  Uint32 alpha;
  
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
  alpha = 0xFF000000;
#elif SDL_BYTEORDER == SDL_LIL_ENDIAN
  alpha = 0x000000FF;
#endif
  
  gif->i = 0;
  gif->tnxt = 0;
  gif->nimg = raw->i;
  
  gif->images = malloc(gif->nimg * sizeof *gif->images);
  if (gif->images == NULL)
    return -1;
  
  gif->delays = malloc(gif->nimg * sizeof *gif->delays);
  if (gif->delays == NULL)
    return -1;
  
  for (i = 0; i < gif->nimg; i++) {
    gif->images[i] = GIF_CreateRGBSurface(raw->w, raw->h);
    if (gif->images[i] == NULL)
      return -1;
    
    gif->delays[i] = raw->img[i].delay;
    
    SDL_FillRect(gif->images[i], NULL, alpha);
    SDL_SetColorKey(gif->images[i], SDL_SRCCOLORKEY, alpha);
  }
  
  return 0;
}

GIF_Surface * GIF_LoadGIF(char * s) {
  GIF_Raw * raw;
  GIF_Surface * gif;
  FILE * f;
  Sint32 sz;
  Uint8 * p;
  
  f = fopen(s, "rb");
  if (f == NULL)
    return NULL;
  
  fseek(f, 0, SEEK_END);
  sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  
  p = malloc(sz * sizeof *p);
  if (p == NULL) {
    perror("GIF_LoadGIF: malloc");
    exit(EXIT_FAILURE);
  }
  
  if (fread(p, sizeof *p, sz, f) != (size_t)sz) {
    fprintf(stderr, "GIF_LoadGIF: fread: File error.\n");
    return NULL;
  }
  
  pdata = p;
  
  raw = malloc(sizeof *raw);
  if (raw == NULL)
    return NULL;
  
  gif = malloc(sizeof *gif);
  if (gif == NULL)
    return NULL;
  
  raw->version = GIF_GetHeader();
  if (raw->version != GIF_87A && raw->version != GIF_89A)
    return NULL;
  
  if (GIF_GetLogScrDescriptor(raw) < 0)
    return NULL;
  
  if (GIF_GetImages(raw) < 0)
    return NULL;
  
  
  
  
  
  if (GIF_InitFrames(raw, gif) < 0)
    return NULL;
  
  if (GIF_RenderFrames(raw, gif) < 0)
    return NULL;
  

	gif->w = raw->w;
	gif->h = raw->h;
  
  
  free(p);
  fclose(f);
  
  return gif;
}

SDL_Surface * GIF_GetNextFrame(GIF_Surface * gif) {
  Uint32 curr = SDL_GetTicks();
  
  if (curr > gif->tnxt) {
    gif->i++;
    
    if (gif->i >= gif->nimg)
      gif->i = 0;
    
    gif->tnxt = SDL_GetTicks() + (gif->delays[gif->i] * 10);
  }
  
  
  return gif->images[gif->i];
}









