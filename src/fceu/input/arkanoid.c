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
#include  "share.h"

typedef struct {
  uint32_t mzx,mzb;
  uint32_t readbit;
} ARK;

static ARK NESArk[2];
static ARK FCArk;

static void StrobeARKFC(void)
{
  FCArk.readbit=0;
}


static uint8_t FP_FASTAPASS(2) ReadARKFC(int w,uint8_t ret)
{
 ret&=~2;

 if (w) 
 {
  if (FCArk.readbit>=8)
   ret|=2;
  else
  {
   ret|=((FCArk.mzx>>(7-FCArk.readbit))&1)<<1;
   if (!fceuindbg)
    FCArk.readbit++;
  }
 }
 else
  ret|=FCArk.mzb<<1;
 return(ret);
}

static uint32_t FixX(uint32_t x)
{
 x=98+x*144/240;
 if (x>242) x=242;
 x=~x;
 return(x);
}

static void FP_FASTAPASS(2) UpdateARKFC(void *data, int arg)
{
 uint32_t *ptr=(uint32_t *)data;
 FCArk.mzx=FixX(ptr[0]);
 FCArk.mzb=ptr[2]?1:0;
}

static INPUTCFC ARKCFC={ReadARKFC,0,StrobeARKFC,UpdateARKFC,0,0};

INPUTCFC *FCEU_InitArkanoidFC(void)
{
 FCArk.mzx=98;
 FCArk.mzb=0;
 return(&ARKCFC);
}

static uint8_t FP_FASTAPASS(1) ReadARK(int w)
{
 uint8_t ret=0;

 if (NESArk[w].readbit>=8)
  ret|=1<<4;
 else
 {
  ret|=((NESArk[w].mzx>>(7-NESArk[w].readbit))&1)<<4;
  if (!fceuindbg)
   NESArk[w].readbit++;
 }
 ret|=(NESArk[w].mzb&1)<<3;
 return(ret);
}


static void FP_FASTAPASS(1) StrobeARK(int w)
{
  NESArk[w].readbit=0;
}

static void FP_FASTAPASS(3) UpdateARK(int w, void *data, int arg)
{
 uint32_t *ptr=(uint32_t*)data;
 NESArk[w].mzx=FixX(ptr[0]);
 NESArk[w].mzb=ptr[2]?1:0;
}

static INPUTC ARKC={ReadARK, 0, StrobeARK, UpdateARK, 0, 0};

INPUTC *FCEU_InitArkanoid(int w)
{
 NESArk[w].mzx=98;
 NESArk[w].mzb=0;
 return(&ARKC);
}
