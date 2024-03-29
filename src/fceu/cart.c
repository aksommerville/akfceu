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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "types.h"
#include "fceu.h"
#include "ppu.h"

#include "cart.h"
#include "memory.h"
#include "x6502.h"

#include "general.h"

/*
   This file contains all code for coordinating the mapping in of the
   address space external to the NES.
   It's also (ab)used by the NSF code.
*/

uint8_t *Page[32],*VPage[8];
uint8_t **VPageR=VPage;
uint8_t *VPageG[8];
uint8_t *MMC5SPRVPage[8];
uint8_t *MMC5BGVPage[8];

static uint8_t PRGIsRAM[32]; /* This page is/is not PRG RAM. */

/* 16 are (sort of) reserved for UNIF/iNES and 16 to map other stuff. */
static int CHRram[32];
static int PRGram[32];

uint8_t *PRGptr[32];
uint8_t *CHRptr[32];

uint32_t PRGsize[32];
uint32_t CHRsize[32];

uint32_t PRGmask2[32];
uint32_t PRGmask4[32];
uint32_t PRGmask8[32];
uint32_t PRGmask16[32];
uint32_t PRGmask32[32];

uint32_t CHRmask1[32];
uint32_t CHRmask2[32];
uint32_t CHRmask4[32];
uint32_t CHRmask8[32];

int geniestage=0;

int modcon;

uint8_t genieval[3];
uint8_t geniech[3];

uint32_t genieaddr[3];

static INLINE void setpageptr(int s, uint32_t A, uint8_t *p, int ram) {
  uint32_t AB=A>>11;
  int x;

  if (p) {
    for (x=(s>>1)-1;x>=0;x--) {
      PRGIsRAM[AB+x]=ram;
      Page[AB+x]=p-A;
    }
  } else {
    for (x=(s>>1)-1;x>=0;x--) {
      PRGIsRAM[AB+x]=0;
      Page[AB+x]=0;
    }
  }
}

static uint8_t nothing[8192];

void ResetCartMapping(void) {
  int x;
  for (x=0;x<32;x++) {
    Page[x]=nothing-x*2048;
    PRGptr[x]=CHRptr[x]=0;
    PRGsize[x]=CHRsize[x]=0;
  }
  for (x=0;x<8;x++) {
    MMC5SPRVPage[x]=MMC5BGVPage[x]=VPageR[x]=nothing-0x400*x;
  }
}

void SetupCartPRGMapping(int chip, uint8_t *p, uint32_t size, int ram) {
  PRGptr[chip]=p;
  PRGsize[chip]=size;

  PRGmask2[chip]=(size>>11)-1;
  PRGmask4[chip]=(size>>12)-1;
  PRGmask8[chip]=(size>>13)-1;
  PRGmask16[chip]=(size>>14)-1;
  PRGmask32[chip]=(size>>15)-1;

  PRGram[chip]=ram?1:0;
}

void SetupCartCHRMapping(int chip, uint8_t *p, uint32_t size, int ram) {
  CHRptr[chip]=p;
  CHRsize[chip]=size;

  CHRmask1[chip]=(size>>10)-1;
  CHRmask2[chip]=(size>>11)-1;
  CHRmask4[chip]=(size>>12)-1;
  CHRmask8[chip]=(size>>13)-1;

  CHRram[chip]=ram;
}

DECLFR(CartBR) {
  return Page[A>>11][A];
}

DECLFW(CartBW) {
  //printf("Ok: %04x:%02x, %d\n",A,V,PRGIsRAM[A>>11]);
  if (PRGIsRAM[A>>11] && Page[A>>11]) {
    Page[A>>11][A]=V;
  }
}

DECLFR(CartBROB) {
  if (!Page[A>>11]) {
    return X.DB;
  }
  return Page[A>>11][A];
}

void FASTAPASS(3) setprg2r(int r, unsigned int A, unsigned int V) {
  V&=PRGmask2[r];

  setpageptr(2,A,PRGptr[r]?(&PRGptr[r][V<<11]):0,PRGram[r]);
}

void FASTAPASS(2) setprg2(uint32_t A, uint32_t V) {
  setprg2r(0,A,V);
}

void FASTAPASS(3) setprg4r(int r, unsigned int A, unsigned int V) {
  V&=PRGmask4[r];
  setpageptr(4,A,PRGptr[r]?(&PRGptr[r][V<<12]):0,PRGram[r]);
}

void FASTAPASS(2) setprg4(uint32_t A, uint32_t V) {
  setprg4r(0,A,V);
}

void FASTAPASS(3) setprg8r(int r, unsigned int A, unsigned int V) {
  if (PRGsize[r]>=8192) {
    V&=PRGmask8[r];
    setpageptr(8,A,PRGptr[r]?(&PRGptr[r][V<<13]):0,PRGram[r]);
  } else {
    uint32_t VA=V<<2;
    int x;
    for (x=0;x<4;x++) {
      setpageptr(2,A+(x<<11),PRGptr[r]?(&PRGptr[r][((VA+x)&PRGmask2[r])<<11]):0,PRGram[r]);
    }
  }
}

void FASTAPASS(2) setprg8(uint32_t A, uint32_t V) {
  setprg8r(0,A,V);
}

void FASTAPASS(3) setprg16r(int r, unsigned int A, unsigned int V) {
  if (PRGsize[r]>=16384) {
    V&=PRGmask16[r];
    setpageptr(16,A,PRGptr[r]?(&PRGptr[r][V<<14]):0,PRGram[r]);
  } else {
    uint32_t VA=V<<3;
    int x;
    for (x=0;x<8;x++) {
      setpageptr(2,A+(x<<11),PRGptr[r]?(&PRGptr[r][((VA+x)&PRGmask2[r])<<11]):0,PRGram[r]);
    }
  }
}

void FASTAPASS(2) setprg16(uint32_t A, uint32_t V) {
  setprg16r(0,A,V);
}

void FASTAPASS(3) setprg32r(int r,unsigned int A, unsigned int V) {
  if (PRGsize[r]>=32768) {
    V&=PRGmask32[r];
    setpageptr(32,A,PRGptr[r]?(&PRGptr[r][V<<15]):0,PRGram[r]);
  } else {
    uint32_t VA=V<<4;
    int x;
    for (x=0;x<16;x++) {
      setpageptr(2,A+(x<<11),PRGptr[r]?(&PRGptr[r][((VA+x)&PRGmask2[r])<<11]):0,PRGram[r]);
    }
  }
}

void FASTAPASS(2) setprg32(uint32_t A, uint32_t V) {
  setprg32r(0,A,V);
}

void FASTAPASS(3) setchr1r(int r, unsigned int A, unsigned int V) {
  if (!CHRptr[r]) return;
  FCEUPPU_LineUpdate();
  V&=CHRmask1[r];
  if (CHRram[r]) {
    PPUCHRRAM|=(1<<(A>>10));
  } else {
    PPUCHRRAM&=~(1<<(A>>10));
  }
  VPageR[(A)>>10]=&CHRptr[r][(V)<<10]-(A);
}

void FASTAPASS(3) setchr2r(int r, unsigned int A, unsigned int V) {
  if (!CHRptr[r]) return;
  FCEUPPU_LineUpdate();
  V&=CHRmask2[r];
  VPageR[(A)>>10]=VPageR[((A)>>10)+1]=&CHRptr[r][(V)<<11]-(A);
  if (CHRram[r]) {
    PPUCHRRAM|=(3<<(A>>10));
  } else {
    PPUCHRRAM&=~(3<<(A>>10));
  }
}

void FASTAPASS(3) setchr4r(int r, unsigned int A, unsigned int V) {
  if (!CHRptr[r]) return;
  FCEUPPU_LineUpdate();
  V&=CHRmask4[r];
  VPageR[(A)>>10]=VPageR[((A)>>10)+1]=
  VPageR[((A)>>10)+2]=VPageR[((A)>>10)+3]=&CHRptr[r][(V)<<12]-(A);
  if (CHRram[r]) {
    PPUCHRRAM|=(15<<(A>>10));
  } else {
    PPUCHRRAM&=~(15<<(A>>10));
  }
}

void FASTAPASS(2) setchr8r(int r, unsigned int V) {
  int x;

  if (!CHRptr[r]) return;
  FCEUPPU_LineUpdate();
  V&=CHRmask8[r];
  for (x=7;x>=0;x--) {
    VPageR[x]=&CHRptr[r][V<<13];
  }
  if (CHRram[r]) {
    PPUCHRRAM|=(255);
  } else {
    PPUCHRRAM&=~(255);
  }
}

void FASTAPASS(2) setchr1(unsigned int A, unsigned int V) {
  setchr1r(0,A,V);
}

void FASTAPASS(2) setchr2(unsigned int A, unsigned int V) {
  setchr2r(0,A,V);
}

void FASTAPASS(2) setchr4(unsigned int A, unsigned int V) {
  setchr4r(0,A,V);
}

void FASTAPASS(1) setchr8(unsigned int V) {
  setchr8r(0,V);
}

void FASTAPASS(1) setvram8(uint8_t *p) {
  int x;
  for (x=7;x>=0;x--) {
    VPageR[x]=p;
  }
  PPUCHRRAM|=255;
}

void FASTAPASS(2) setvram4(uint32_t A, uint8_t *p) {
  int x;
  for (x=3;x>=0;x--) {
    VPageR[(A>>10)+x]=p-A;
  }
  PPUCHRRAM|=(15<<(A>>10));
}

void FASTAPASS(3) setvramb1(uint8_t *p, uint32_t A, uint32_t b) {
  FCEUPPU_LineUpdate();
  VPageR[A>>10]=p-A+(b<<10);
  PPUCHRRAM|=(1<<(A>>10));
}

void FASTAPASS(3) setvramb2(uint8_t *p, uint32_t A, uint32_t b) {
  FCEUPPU_LineUpdate();
  VPageR[(A>>10)]=VPageR[(A>>10)+1]=p-A+(b<<11);
  PPUCHRRAM|=(3<<(A>>10));
}

void FASTAPASS(3) setvramb4(uint8_t *p, uint32_t A, uint32_t b) {
  int x;
  FCEUPPU_LineUpdate();
  for (x=3;x>=0;x--) {
    VPageR[(A>>10)+x]=p-A+(b<<12);
  }
  PPUCHRRAM|=(15<<(A>>10));
}

void FASTAPASS(2) setvramb8(uint8_t *p, uint32_t b) {
  int x;
  FCEUPPU_LineUpdate();
  for (x=7;x>=0;x--) {
    VPageR[x]=p+(b<<13);
  }
  PPUCHRRAM|=255;
}

/* This function can be called without calling SetupCartMirroring(). */

void FASTAPASS(3) setntamem(uint8_t *p, int ram, uint32_t b) {
  FCEUPPU_LineUpdate();
  vnapage[b]=p;
  PPUNTARAM&=~(1<<b);
  if (ram) {
    PPUNTARAM|=1<<b;
  }
}

static int mirrorhard=0;

void setmirrorw(int a, int b, int c, int d) {
  FCEUPPU_LineUpdate();
  vnapage[0]=NTARAM+a*0x400;
  vnapage[1]=NTARAM+b*0x400;
  vnapage[2]=NTARAM+c*0x400;
  vnapage[3]=NTARAM+d*0x400;
}

void FASTAPASS(1) setmirror(int t) {
  FCEUPPU_LineUpdate();
  if (!mirrorhard) {
    switch (t) {
      case MI_H: {
          vnapage[0]=vnapage[1]=NTARAM;vnapage[2]=vnapage[3]=NTARAM+0x400;
        } break;
      case MI_V: {
          vnapage[0]=vnapage[2]=NTARAM;vnapage[1]=vnapage[3]=NTARAM+0x400;
        } break;
      case MI_0: {
          vnapage[0]=vnapage[1]=vnapage[2]=vnapage[3]=NTARAM;
        } break;
      case MI_1: {
          vnapage[0]=vnapage[1]=vnapage[2]=vnapage[3]=NTARAM+0x400;
        } break;
    }
    PPUNTARAM=0xF;
  }
}

void SetupCartMirroring(int m, int hard, uint8_t *extra) {
  if (m<4) {
    mirrorhard = 0;
    setmirror(m);
  } else {
    vnapage[0]=NTARAM;
    vnapage[1]=NTARAM+0x400;
    vnapage[2]=extra;
    vnapage[3]=extra+0x400;
    PPUNTARAM=0xF;
  }
  mirrorhard = hard;
}

void FCEU_SaveGameSave(CartInfo *LocalHWInfo) {
  if (LocalHWInfo->battery && LocalHWInfo->SaveGame[0]) {
    FILE *sp;
    char path[1024];
    int pathc;

    pathc=FCEU_MakePath(path,sizeof(path),FCEUMKF_SAV,0,"sav");
    if ((pathc<1)||(pathc>=sizeof(path))) return;
    if ((sp=FCEUD_UTF8fopen(path,"wb"))==NULL) {
      FCEU_PrintError("WRAM file \"%s\" cannot be written to.\n",path);
    } else {
      int x;
 
      for (x=0;x<4;x++) {
        if (LocalHWInfo->SaveGame[x]) {
          fwrite(LocalHWInfo->SaveGame[x],1,LocalHWInfo->SaveGameLen[x],sp);
        }
      }
    }
  }
}
  
void FCEU_LoadGameSave(CartInfo *LocalHWInfo) {
  if (LocalHWInfo->battery && LocalHWInfo->SaveGame[0]) {
    FILE *sp;
    char path[1024];
    int pathc;
 
    pathc=FCEU_MakePath(path,sizeof(path),FCEUMKF_SAV,0,"sav");
    if ((pathc<1)||(pathc>=sizeof(path))) return;
    sp=FCEUD_UTF8fopen(path,"rb");
    if (sp!=NULL) {
      int x;
      for (x=0;x<4;x++) {
        if (LocalHWInfo->SaveGame[x]) {
          fread(LocalHWInfo->SaveGame[x],1,LocalHWInfo->SaveGameLen[x],sp);
        }
      }
    }
  }
}
