/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2003 Xodnizel
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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "types.h"
#include "x6502.h"
#include "fceu.h"
#include "ppu.h"
#include "sound.h"
#include "general.h"
#include "endian.h"
#include "memory.h"
#include "cart.h"
#include "fds.h"
#include "ines.h"
#include "unif.h"
#include "palette.h"
#include "state.h"
#include "input.h"
#include "file.h"
#include "crc32.h"
#include "vsuni.h"

void FCEUD_NetworkClose();

uint64_t timestampbase;

FCEUGI *FCEUGameInfo = NULL;
void (*GameInterface)(int h);

void (*GameStateRestore)(int version);

readfunc ARead[0x10000];
writefunc BWrite[0x10000];
static readfunc *AReadG;
static writefunc *BWriteG;
static int RWWrap=0;

static DECLFW(BNull) {
}

static DECLFR(ANull) {
  return(X.DB);
}

readfunc FASTAPASS(1) GetReadHandler(int32_t a) {
  if (a>=0x8000 && RWWrap) {
    return AReadG[a-0x8000];
  } else {
    return ARead[a];
  }
}

void FASTAPASS(3) SetReadHandler(int32_t start, int32_t end, readfunc func) {
  int32_t x;

  if (!func) func=ANull;

  if (RWWrap) {
    for (x=end;x>=start;x--) {
      if (x>=0x8000) {
        AReadG[x-0x8000]=func;
      } else {
        ARead[x]=func;
      }
    }
  } else {
    for (x=end;x>=start;x--) {
      ARead[x]=func;
    }
  }
}

writefunc FASTAPASS(1) GetWriteHandler(int32_t a) {
  if (RWWrap && a>=0x8000) {
    return BWriteG[a-0x8000];
  } else {
    return BWrite[a];
  }
}

void FASTAPASS(3) SetWriteHandler(int32_t start, int32_t end, writefunc func) {
  int32_t x;

  if (!func) func=BNull;

  if (RWWrap) {
    for (x=end;x>=start;x--) {
      if (x>=0x8000) {
        BWriteG[x-0x8000]=func;
      } else {
        BWrite[x]=func;
      }
    }
  } else {
    for (x=end;x>=start;x--) {
      BWrite[x]=func;
    }
  }
}

uint8_t GameMemBlock[131072];
uint8_t RAM[0x800];
uint8_t PAL=0;

static DECLFW(BRAML) {
  RAM[A]=V;
}

static DECLFW(BRAMH) {
  RAM[A&0x7FF]=V;
}

static DECLFR(ARAML) {
  return RAM[A];
}

static DECLFR(ARAMH) {
  return RAM[A&0x7FF];
}

static void CloseGame(void) {
  if (FCEUGameInfo) {
    if (FCEUGameInfo->name) {
      free(FCEUGameInfo->name);
      FCEUGameInfo->name=0;
    }
    GameInterface(GI_CLOSE);
    ResetExState(0,0);
    free(FCEUGameInfo);
    FCEUGameInfo = 0;
  }
}

void ResetGameLoaded(void) {
  if (FCEUGameInfo) CloseGame();
  GameStateRestore=0;
  PPU_hook=0;
  GameHBIRQHook=0;
  if (GameExpSound.Kill) GameExpSound.Kill();
  memset(&GameExpSound,0,sizeof(GameExpSound));
  MapIRQHook=0;
  MMC5Hack=0;
  PAL&=1;
  pale=0;
}

int UNIFLoad(const char *name, FCEUFILE *fp);
int iNESLoad(const char *name, FCEUFILE *fp);
int FDSLoad(const char *name, FCEUFILE *fp);

FCEUGI *FCEUI_LoadGame(const char *name) {
  FCEUFILE *fp;
  char ipsfn[1024];
  int ipsfnc;

  ResetGameLoaded();

  FCEUGameInfo = malloc(sizeof(FCEUGI));
  memset(FCEUGameInfo, 0, sizeof(FCEUGI));

  FCEUGameInfo->soundchan = 0;
  FCEUGameInfo->soundrate = 0;
  FCEUGameInfo->name=0;
  FCEUGameInfo->type=GIT_CART;
  FCEUGameInfo->vidsys=GIV_USER;
  FCEUGameInfo->input[0]=FCEUGameInfo->input[1]=-1;
  FCEUGameInfo->inputfc=-1;
  FCEUGameInfo->cspecial=0;

  FCEU_printf("Loading %s...\n\n",name);

  GetFileBase(name);

  ipsfnc=FCEU_MakePath(ipsfn,sizeof(ipsfn),FCEUMKF_IPS,0,0);
  if ((ipsfnc<1)||(ipsfnc>=sizeof(ipsfn))) return 0;
  fp=FCEU_fopen(name,ipsfn,"rb",0);

  if (!fp) {
    FCEU_PrintError("Error opening \"%s\"!",name);
    return 0;
  }

  if (iNESLoad(name,fp)) goto endlseq;
  if (UNIFLoad(name,fp)) goto endlseq;
  if (FDSLoad(name,fp)) goto endlseq;

  FCEU_PrintError("An error occurred while loading the file.");
  FCEU_fclose(fp);
  return 0;

 endlseq:
  FCEU_fclose(fp);

  FCEU_ResetVidSys();

  PowerNES();
  FCEUSS_CheckStates();

  FCEU_LoadGamePalette();
       
  FCEU_ResetPalette();

  return FCEUGameInfo;
}

// Yoinked from video.c, which i plan to delete -aks
uint8_t *XBuf=NULL;
static int FCEU_InitVirtualVideo() {
  if (!XBuf) {
    if (!(XBuf=malloc(256*256))) return 0;
  }
  memset(XBuf,0x80,256*256);
  return 1;
}

int FCEUI_Initialize(void) {
  if (!FCEU_InitVirtualVideo()) return 0;
  memset(&FSettings,0,sizeof(FSettings));
  FSettings.UsrFirstSLine[0]=8;
  FSettings.UsrFirstSLine[1]=0;
  FSettings.UsrLastSLine[0]=231;
  FSettings.UsrLastSLine[1]=239;
  FSettings.SoundVolume=100;
  FCEUPPU_Init();
  X6502_Init();
  return 1;
}

void FCEUI_Kill(void) {
}

void FCEUI_Emulate(uint8_t **pXBuf, int32_t **SoundBuf, int32_t *SoundBufSize, int skip) {
  int r,ssize;

  FCEU_UpdateInput();
  r=FCEUPPU_Loop(skip);

  ssize=FlushEmulateSound();

  timestampbase += timestamp;

  timestamp = 0;
  
  *pXBuf=skip?0:XBuf;
  *SoundBuf=WaveFinal;
  *SoundBufSize=ssize;
}

void FCEUI_CloseGame(void) {
  CloseGame();
}

void ResetNES(void) {
  if (!FCEUGameInfo) return;
  GameInterface(GI_RESETM2);
  FCEUSND_Reset();
  FCEUPPU_Reset();
  X6502_Reset();
}

void FCEU_MemoryRand(uint8_t *ptr, uint32_t size) {
  int x=0;
  while (size) {
    *ptr=(x&4)?0xFF:0x00;
    x++;
    size--;
    ptr++;
  }
}

void hand(X6502 *X, int type, unsigned int A) {
}

void PowerNES(void) {
  if (!FCEUGameInfo) return;

  FCEU_MemoryRand(RAM,0x800);
  //memset(RAM,0xFF,0x800);

  SetReadHandler(0x0000,0xFFFF,ANull);
  SetWriteHandler(0x0000,0xFFFF,BNull);
       
  SetReadHandler(0,0x7FF,ARAML);
  SetWriteHandler(0,0x7FF,BRAML);
       
  SetReadHandler(0x800,0x1FFF,ARAMH);  /* Part of a little */
  SetWriteHandler(0x800,0x1FFF,BRAMH); /* hack for a small speed boost. */

  InitializeInput();
  FCEUSND_Power();
  FCEUPPU_Power();

  /* Have the external game hardware "powered" after the internal NES stuff. 
     Needed for the NSF code and VS System code.
  */
  GameInterface(GI_POWER);
  if (FCEUGameInfo->type==GIT_VSUNI) {
    FCEU_VSUniPower();
  }

  timestampbase=0;
  X6502_Power();
}

void FCEU_ResetVidSys(void) {
  int w;
 
  if (FCEUGameInfo->vidsys==GIV_NTSC) w=0;
  else if (FCEUGameInfo->vidsys==GIV_PAL) w=1; 
  else w=FSettings.PAL;
 
  PAL=w?1:0;
  FCEUPPU_SetVideoSystem(w);
  SetSoundVariables();
}

FCEUS FSettings;

void FCEU_printf(char *format, ...) {
  char temp[2048];

  va_list ap;

  va_start(ap,format);
  vsnprintf(temp,sizeof(temp),format,ap);
  FCEUD_Message(temp);

  va_end(ap);
}

void FCEU_PrintError(char *format, ...) {
  char temp[2048];

  va_list ap;

  va_start(ap,format);
  vsnprintf(temp,sizeof(temp),format,ap);
  FCEUD_PrintError(temp);

  va_end(ap);
}

void FCEUI_SetRenderedLines(int ntscf, int ntscl, int palf, int pall) {
  FSettings.UsrFirstSLine[0]=ntscf;
  FSettings.UsrLastSLine[0]=ntscl;
  FSettings.UsrFirstSLine[1]=palf;
  FSettings.UsrLastSLine[1]=pall;
  if (PAL) {
    FSettings.FirstSLine=FSettings.UsrFirstSLine[1];
    FSettings.LastSLine=FSettings.UsrLastSLine[1];
  } else {
    FSettings.FirstSLine=FSettings.UsrFirstSLine[0];
    FSettings.LastSLine=FSettings.UsrLastSLine[0];
  }
}

void FCEUI_SetVidSystem(int a) {
  FSettings.PAL=a?1:0;
  if (FCEUGameInfo) {
    FCEU_ResetVidSys();
    FCEU_ResetPalette();
  }
}

int FCEUI_GetCurrentVidSystem(int *slstart, int *slend) {
  if (slstart) *slstart=FSettings.FirstSLine;
  if (slend) *slend=FSettings.LastSLine;
  return PAL;
}

void FCEUI_SetSnapName(int a) {
  FSettings.SnapName=a;
}

int32_t FCEUI_GetDesiredFPS(void) {
  if (PAL) {
    return(838977920); // ~50.007
  } else {
    return(1008307711);  // ~60.1
  }
}
