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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "types.h"
#include "fceu.h"
#include "general.h"
#include "state.h"
#include "driver.h"
#include "md5.h"

static char BaseDirectory[2048];
static char FileBase[2048];
static char FileExt[2048];  /* Includes the . character, as in ".nes" */

static char FileBaseDirectory[2048];

static void mkdir_if_needed(const char *dir,const char *base) {
  if (base) {
    char sub[1024];
    int subc=snprintf(sub,sizeof(sub),"%s"PSS"%s",dir,base);
    if ((subc>0)&&(subc<sizeof(sub))) {
      mkdir_if_needed(sub,0);
    }
  }
  mkdir(dir,0775);
}

void FCEUI_SetBaseDirectory(const char *dir) {
  strncpy(BaseDirectory,dir,2047);
  BaseDirectory[2047]=0;

  // aks: Create the directory structure, if it's not there already.
  mkdir_if_needed(BaseDirectory,0);
  mkdir_if_needed(BaseDirectory,"fcs");
  mkdir_if_needed(BaseDirectory,"snaps");
  mkdir_if_needed(BaseDirectory,"sav");
  mkdir_if_needed(BaseDirectory,"gameinfo");
}

int FCEU_MakePath(char *dst,int dsta,int type,int id1,const char *cd1) {
  if (!dst||(dsta<1)) return -1;
  switch (type) {
  
    case FCEUMKF_STATE: {
        int dstc=snprintf(dst,dsta,"%s"PSS"fcs"PSS"%s.fc%d",BaseDirectory,FileBase,id1);
        if ((dstc<1)||(dstc>=dsta)) return dstc;
        struct stat st;
        if (stat(dst,&st)<0) {
          dstc=snprintf(dst,dsta,"%s"PSS"fcs"PSS"%s.%s.fc%d",BaseDirectory,FileBase,md5_asciistr(FCEUGameInfo->MD5),id1);
        }
        return dstc;
      }
      
    case FCEUMKF_FDS: {
        return snprintf(dst,dsta,"%s"PSS"sav"PSS"%s.%s.fds",BaseDirectory,FileBase,md5_asciistr(FCEUGameInfo->MD5));
      }
      
    case FCEUMKF_SAV: {
        int dstc=snprintf(dst,dsta,"%s"PSS"sav"PSS"%s.%s",BaseDirectory,FileBase,cd1);
        if ((dstc<1)||(dstc>=dsta)) return dstc;
        struct stat st;
        if (stat(dst,&st)<0) {
          dstc=snprintf(dst,dsta,"%s"PSS"sav"PSS"%s.%s.%s",BaseDirectory,FileBase,md5_asciistr(FCEUGameInfo->MD5),cd1);
        }
        return dstc;
      }

    case FCEUMKF_IPS: {
        return snprintf(dst,dsta,"%s"PSS"%s%s.ips",FileBaseDirectory,FileBase,FileExt);
      }

    case FCEUMKF_FDSROM: {
        return snprintf(dst,dsta,"%s"PSS"disksys.rom",BaseDirectory);
      }

    case FCEUMKF_PALETTE: {
        return snprintf(dst,dsta,"%s"PSS"gameinfo"PSS"%s.pal",BaseDirectory,FileBase);
      }

    case FCEUMKF_CONFIG: {
        return snprintf(dst,dsta,"%s"PSS"akfceu.cfg",BaseDirectory);
      }
  
  }
  return -1;
}

void GetFileBase(const char *f) {
  const char *tp1,*tp3;

  #if PSS_STYLE==4
    tp1=((char *)strrchr(f,':'));
  #elif PSS_STYLE==1
    tp1=((char *)strrchr(f,'/'));
  #else
    tp1=((char *)strrchr(f,'\\'));
    #if PSS_STYLE!=3
      tp3=((char *)strrchr(f,'/'));
      if (tp1<tp3) tp1=tp3;
    #endif
  #endif
  
  if (!tp1) {
    tp1=f;
    strcpy(FileBaseDirectory,".");
  } else {
    memcpy(FileBaseDirectory,f,tp1-f);
    FileBaseDirectory[tp1-f]=0;
    tp1++;
  }

  if (((tp3=strrchr(f,'.'))!=NULL) && (tp3>tp1)) {
    memcpy(FileBase,tp1,tp3-tp1);
    FileBase[tp3-tp1]=0;
    strcpy(FileExt,tp3);
  } else {
    strcpy(FileBase,tp1);
    FileExt[0]=0;
  }
}

uint32_t uppow2(uint32_t n) {
  int x;
  for (x=31;x>=0;x--) {
    if (n&(1<<x)) {
      if ((1<<x)!=n) return(1<<(x+1));
      break;
    }
  }
  return n;
}

