/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2002 Xodnizel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include        <string.h>
#include  <stdlib.h>

#include        "share.h"

typedef struct {
        uint32_t mzx,mzy,mzb;
        int zap_readbit;
        int bogo;
        int zappo;
        uint64_t zaphit;
} ZAPPER;

static ZAPPER ZD;

static void FP_FASTAPASS(3) ZapperFrapper(uint8_t *bg, uint8_t *spr, uint32_t  linets, int final)
{
 int xs,xe;  
 int zx,zy;

 if (!bg) // New line, so reset stuff.
 {
  ZD.zappo=0;
  return;
 }
 xs=ZD.zappo; 
 xe=final;

 zx=ZD.mzx;
 zy=ZD.mzy;

 if (xe>256) xe=256;

 if (scanline>=(zy-4) && scanline<=(zy+4))
 {
  while (xs<xe)
  {
    uint8_t a1,a2;
    uint32_t sum;
    if (xs<=(zx+4) && xs>=(zx-4))
    {
     a1=bg[xs];
     if (spr)
     {
      a2=spr[xs]; 

      if (!(a2&0x80))
       if (!(a2&0x40) || (a1&64))
        a1=a2;
     }
     a1&=63;

     sum=palo[a1].r+palo[a1].g+palo[a1].b;
     if (sum>=100*3)
     {
      ZD.zaphit=((uint64_t)linets+(xs+16)*(PAL?15:16))/48+timestampbase;
      goto endo;
     }
    }
   xs++;
  } 
 }
 endo:
 ZD.zappo=final;
}

static INLINE int CheckColor(void)
{
 FCEUPPU_LineUpdate();

 if ((ZD.zaphit+10)>=(timestampbase+timestamp)) return(0);

 return(1);  
}


static uint8_t FP_FASTAPASS(2) ReadZapper(int w, uint8_t ret)
{
    if (w)
    {
     ret&=~0x18;
                 if (ZD.bogo)
                  ret|=0x10;
                 if (CheckColor())
                  ret|=0x8;
    }
    else
    {
     //printf("Kayo: %d\n",ZD.zap_readbit);
     ret&=~2;
     //if(ZD.zap_readbit==4) ret|=ZD.mzb&2;
     ret|=(ret&1)<<1;
     //ZD.zap_readbit++;
    }
                return ret;
}

static void FP_FASTAPASS(2) DrawZapper(uint8_t *buf, int arg)
{
 if (arg)
  FCEU_DrawCursor(buf, ZD.mzx, ZD.mzy);
}

static void FP_FASTAPASS(2) UpdateZapper(void *data, int arg)
{
  uint32_t *ptr=(uint32_t*)data;

  if (ZD.bogo)
   ZD.bogo--;
  if (ptr[2]&1 && (!(ZD.mzb&1)))
   ZD.bogo=5;

  ZD.mzx=ptr[0];
  ZD.mzy=ptr[1];
  ZD.mzb=ptr[2];
}

static void StrobeShadow(void)
{
 ZD.zap_readbit=0;
}

static INPUTCFC SHADOWC={ReadZapper,0,StrobeShadow,UpdateZapper,ZapperFrapper,DrawZapper};

INPUTCFC *FCEU_InitSpaceShadow(void)
{
  memset(&ZD,0,sizeof(ZAPPER));
  return(&SHADOWC);
}


