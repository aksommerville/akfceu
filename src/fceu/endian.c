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

/*  Contains file I/O functions that write/read data    */
/*  LSB first.                    */

#include <stdio.h>
#include "types.h"
#include "endian.h"

void FlipByteOrder(uint8_t *src, uint32_t count) {
  uint8_t *start=src;
  uint8_t *end=src+count-1;

  if ((count&1) || !count) return; /* This shouldn't happen. */

  while (count--) {
    uint8_t tmp;
    tmp=*end;
    *end=*start;
    *start=tmp;
    end--;
    start++;
  }
}

int write16le(uint16_t b, FILE *fp) {
  uint8_t s[2];
  s[0]=b;
  s[1]=b>>8;
  return ((fwrite(s,1,2,fp)<2)?0:2);
}

int write32le(uint32_t b, FILE *fp) {
  uint8_t s[4];
  s[0]=b;
  s[1]=b>>8;
  s[2]=b>>16;
  s[3]=b>>24;
  return ((fwrite(s,1,4,fp)<4)?0:4);
}

int read32le(uint32_t *Bufo, FILE *fp) {
  uint32_t buf;
  if (fread(&buf,1,4,fp)<4) return 0;
  #ifdef LSB_FIRST
    *(uint32_t*)Bufo=buf;
  #else
    *(uint32_t*)Bufo=((buf&0xFF)<<24)|((buf&0xFF00)<<8)|((buf&0xFF0000)>>8)|((buf&0xFF000000)>>24);
  #endif
  return 1;
}

int read16le(char *d, FILE *fp) {
  #ifdef LSB_FIRST
    return((fread(d,1,2,fp)<2)?0:2);
  #else
    int ret;
    ret=fread(d+1,1,1,fp);
    ret+=fread(d,1,1,fp);
    return ret<2?0:2;
  #endif
}

void FCEU_en32lsb(uint8_t *buf, uint32_t morp) {
  buf[0]=morp;
  buf[1]=morp>>8;
  buf[2]=morp>>16;
  buf[3]=morp>>24;
}

uint32_t FCEU_de32lsb(uint8_t *morp) {
  return(morp[0]|(morp[1]<<8)|(morp[2]<<16)|(morp[3]<<24));
}
