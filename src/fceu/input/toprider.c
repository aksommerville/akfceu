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
#include "share.h"

static uint32_t bs,bss;
static uint32_t boop;

static uint8_t FP_FASTAPASS(2) Read(int w, uint8_t ret)
{
 if (w)
 {
  ret|=(bs&1)<<3;
  ret|=(boop&1)<<4;
  bs>>=1; 
  boop>>=1;
//  puts("Read");
 }
 return(ret);
}

static void FP_FASTAPASS(1) Write(uint8_t V)
{
 // if (V&0x2)
 bs=bss;
 //printf("Write: %02x\n",V);
// boop=0xC0;
}

static void FP_FASTAPASS(2) Update(void *data, int arg)
{
 bss=*(uint8_t*)data;
 bss|=bss<<8;
 bss|=bss<<8;
}

static INPUTCFC TopRider={Read,Write,0,Update,0,0};

INPUTCFC *FCEU_InitTopRider(void)
{

 return(&TopRider);
}

