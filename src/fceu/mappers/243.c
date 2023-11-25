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

#include "mapinc.h"

/* Mapper 243. Only used by Honey Peach.
 * Hint: Keep RAM 0x56 zero and you'll never lose.
 * Emulation is incorrect but mostly works? I gave up after an hour or so.
 */

// Copied and adjusted from:
// sachen.c:S8259

static uint8_t cmd;
static uint8_t latch[8];

static int type;
static void Mapper243_Synco(void)
{
 int x;

  setprg32(0x8000,latch[5]&3);

    // This seems to be incorrect, but it's as close as i'm getting -ak
  int a=((latch[2]&0x01)<<3)|((latch[4]&0x03)<<0)|((latch[6]&0x02)<<1);
  setchr8(a);
    
 switch ((latch[7]>>1)&3)
 {
  case 0:setmirrorw(0,0,0,1);break;
  case 1:setmirror(MI_H);break;
  case 2:setmirror(MI_V);break;
  case 3:setmirror(MI_0);break;
 }
}

static DECLFW(Mapper243_Write)
{
 A&=0x4101;
 if (A==0x4100) cmd=V;
 else
 {
  latch[cmd&7]=V;
  Mapper243_Synco();
 }
}

static void Mapper243_Reset(void)
{
 int x;
 cmd=0;

 for (x=0;x<8;x++) latch[x]=0;

 Mapper243_Synco();
 SetReadHandler(0x8000,0xFFFF,CartBR);
 SetWriteHandler(0x4100,0x7FFF,Mapper243_Write);
}

static void Mapper243_Restore(int version)
{
 Mapper243_Synco();
}

void Mapper243_Init(CartInfo *info)
{
 info->Power=Mapper243_Reset;
 GameStateRestore=Mapper243_Restore;
 AddExState(latch, 8, 0, "LATC");
 AddExState(&cmd, 1, 0, "CMD");
}
