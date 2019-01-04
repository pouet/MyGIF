#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL.h>

#include "GIF_Struct.h"
#include "GIF_LZW.h"


enum {
  GIF_LZW_DICSIZE = 4096
};

typedef struct {
  struct {
    Uint8 c;    /* character */
    Uint16 i;   /* index for previous index */
  } dic[GIF_LZW_DICSIZE];
  
  Uint16 i;           /* Index in 'dic' */
  Uint16 cdeSz;       /* Code size (bit) */
  Uint16 minCdeSz;    /* Minimum code size */
  Uint16 clearCode;   /* Clear code : 2**<code size> */
  Uint16 endOfInfo;   /* End of Information : <clear code> + 1 */
  
} GIF_LZW_Dic;

typedef struct {
  Uint8 * buf;
  Uint8 sz;       /* Size of the buffer */
  Uint16 i;       /* Index in the buffer */
  Uint8 bit;      /* Current bit */
} GIF_LZW_Buf;



int GIF_LZW_DicInit(GIF_LZW_Dic * dic, unsigned codeSize) {
  
  dic->cdeSz = codeSize + 1;
  dic->minCdeSz = codeSize;
  dic->clearCode = 1 << codeSize;
  dic->endOfInfo = dic->clearCode + 1;
  dic->i = dic->clearCode + 2;
  
  return 0;
}

int GIF_LZW_DicReset(GIF_LZW_Dic * dic) {
  
  dic->cdeSz = dic->minCdeSz + 1;
  dic->i = dic->clearCode + 2;
  
  return 0;
}

int GIF_LZW_DicCheckCdeSize(GIF_LZW_Dic * dic) {
  Uint16 n = 1 << dic->cdeSz;
  
  if (dic->i >= n && dic->cdeSz < 12) {
    dic->cdeSz++;
  }
  
  return 0;
}

int GIF_LZW_DicAddCode(GIF_LZW_Dic * dic, Uint16 c, Uint16 oldCode) {
  if (dic->i >= GIF_LZW_DICSIZE) {
    fprintf(stderr, "GIF_LZW_DicAddCode : Table full.\n");
    return -1;
  }
  
  dic->dic[dic->i].c = c;
  dic->dic[dic->i].i = oldCode;
  dic->i++;
  
  return 0;
}

Uint16 GIF_LZW_DicGetTranslation(Uint16 * s, GIF_LZW_Dic * dic, Uint16 code) {
  Uint16 i = 0;
  
  if (code > dic->endOfInfo) {
    i = GIF_LZW_DicGetTranslation(s, dic, dic->dic[code].i);
    s[i] = dic->dic[code].c;
  }
  else
    s[i] = code;
  
  return i + 1;
}

Uint16 * GIF_LZW_DicStrOutput(Uint16 * p, Uint16 * s, Uint16 n) {
  
  while (n != 0) {
    *p++ = *s++;
    n--;
  }
  
  return p;
}

int GIF_LZW_SetBuffer(GIF_LZW_Buf * buf) {
//  Uint8 tmp[256];
//  long pos;
  Uint32 sz;
  Uint8 * p = pdata;
  
  sz = 0;
  
  while (1) {
    if (*p == 0)
      break;
    else
      sz += *p;
    
    p += (*p) + 1;
    
//    if (fread(&buf->sz, sizeof buf->sz, 1, f) != 1)
//      return -1;
//    /* Block terminator */
//    if (buf->sz == 0)
//      break;
//    sz += buf->sz;
//    if (fread(tmp, sizeof *tmp, buf->sz, f) != buf->sz)
//      return -1;
  }
  
  buf->buf = malloc((sz + 1) * sizeof *buf->buf);
  if (buf->buf == NULL) {
    perror("GIF_LZW_SetBuffer : malloc");
    return -1;
  }
  
  sz = 0;
  while (1) {
    if (*pdata == 0)
      break;
    else {
      Uint16 i;
      Uint8 n;
      
      for (n = *pdata++, i = 0; i < n; i++, sz++) {
        buf->buf[sz] = *pdata++;
      }
    }
    
//    if (fread(&buf->sz, sizeof buf->sz, 1, f) != 1)
//      return -1;
//    /* Block terminator */
//    if (buf->sz == 0)
//      break;
//    if (fread(buf->buf + sz, sizeof *buf->buf, buf->sz, f) != buf->sz)
//      return -1;
//    sz += buf->sz;
  }
  
  pdata++;
  
  buf->bit = 0;
  buf->i = 0;
  
  return 0;
}

int GIF_LZW_ClearBuffer(GIF_LZW_Buf * buf) {
  free(buf->buf);
  return 0;
}

/* Thanks 17o2!! */
Uint16 GIF_LZW_GetBits(GIF_LZW_Buf * buf, unsigned nBits) {
	Uint16	ret = 0;
	Uint32	i;
	Uint8	mask;
	Uint16	maskd;
  
  //printf("%02X %02X / nbits = %d / ", buf->buf[buf->i], buf->buf[buf->i+1], nBits);
  
	mask = 1 << buf->bit;
	maskd = 1;
	for (i = 0; i < nBits; i++)
	{
		ret |= (buf->buf[buf->i] & mask ? maskd : 0);
		maskd <<= 1;
		mask <<= 1;
		if (mask == 0)
		{
			mask = 1;
			buf->i++;
		}
	}
	buf->bit = (buf->bit + nBits) & 7;
  
  //printf("code = %x\n", ret);
  
	return (ret);
}


int GIF_LZW_GetData(GIF_Image * img) {
  GIF_LZW_Dic dic;
  GIF_LZW_Buf buf;
  Uint16 oldCode;
  Uint16 code;
  Uint8 tmp;
  Uint16 * p;
  Uint16 s[GIF_LZW_DICSIZE + 1];
  Uint16 n;
  
//  printf("w : %d\n", img->imgWidth);
//  printf("h : %d\n", img->imgHeight);
//  printf("nbpix : %d\n", img->imgWidth * img->imgHeight);
  
  img->data = malloc(img->imgHeight * img->imgWidth * sizeof *img->data);
  if (img->data == NULL)
    return -1;
  
  /* Read the minimum code size */
  tmp = *pdata++;
  
  GIF_LZW_DicInit(&dic, tmp);
  GIF_LZW_SetBuffer(&buf);
  
  p = img->data;
  
  code = GIF_LZW_GetBits(&buf, dic.cdeSz);
  if (code != dic.clearCode) {
    fprintf(stderr, "GIF_LZW_GetData : First code must be a clear code.\n");
    GIF_LZW_ClearBuffer(&buf);
    return -1;
  }
  
  /*oldCode = GIF_LZW_GetBits(&buf, dic.cdeSz);
  n = GIF_LZW_DicGetTranslation(s, &dic, oldCode);
  pdata = GIF_LZW_DicStrOutput(pdata, s, n);*/
  
  goto reset;
  
  while (1) {
    //printf("%d / ", dic.i);
    code = GIF_LZW_GetBits(&buf, dic.cdeSz);
    
    if (code == dic.clearCode) reset: {
      GIF_LZW_DicReset(&dic);
      code = GIF_LZW_GetBits(&buf, dic.cdeSz);
      n = GIF_LZW_DicGetTranslation(s, &dic, code);
      p = GIF_LZW_DicStrOutput(p, s, n);
    }
    else if (code == dic.endOfInfo) {
      break;
    }
    else {
      /* The code is present */
      if (code < dic.i) {
        n = GIF_LZW_DicGetTranslation(s, &dic, code);
      }
      /* The code is not present */
      else if (code != dic.i) {
        fprintf(stderr, "GIF_LZW_GetData : Unkonwn code.\n");
        GIF_LZW_ClearBuffer(&buf);
        return -1;
      }
      /* Error */
      else {
        n = GIF_LZW_DicGetTranslation(s, &dic, oldCode);
        s[n++] = s[0];
      }
      
      p = GIF_LZW_DicStrOutput(p, s, n);
      GIF_LZW_DicAddCode(&dic, s[0], oldCode);
      GIF_LZW_DicCheckCdeSize(&dic);
    }
    
    oldCode = code;
  }
  
  GIF_LZW_ClearBuffer(&buf);
  
  return 0;
}

