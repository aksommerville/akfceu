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

static uint8_t lastA;
static uint8_t DRegs[8];
static uint8_t cmd;
static uint8_t MirCache[8];

static SFORMAT DB_StateRegs[]={
  {DRegs, 8, "DREG"},
  {&cmd, 1, "CMD"},
  {&lastA, 1, "LAST"},
  {0}
};

static void FP_FASTAPASS(1) dragonbust_ppu(uint32_t A)
{
 static int last=-1;
 static uint8_t z;

 if (A>=0x2000) return;

 A>>=10;

 lastA=A; 

 z=MirCache[A];
 if (z!=last)
 {
  onemir(z);
  last=z;
 }
}

static void toot(void)
{
 int x;

 MirCache[0]=MirCache[1]=(DRegs[0]>>4)&1;
 MirCache[2]=MirCache[3]=(DRegs[1]>>4)&1;

 for (x=0;x<4;x++)
  MirCache[4+x]=(DRegs[2+x]>>5)&1;
 onemir(MirCache[lastA]);
}

static DECLFW(Mapper95_write)
{
        switch (A&0xF001)
  {

         case 0x8000:
         cmd = V;
         break;

         case 0x8001:
                switch (cmd&0x07)
    {
                 case 0: DRegs[0]=(V&0x3F)>>1;toot();V>>=1;setchr2(0x0000,V&0x1F);break;
                 case 1: DRegs[1]=(V&0x3F)>>1;toot();V>>=1;setchr2(0x0800,V&0x1F);break;
                 case 2: DRegs[2]=V&0x3F;toot();setchr1(0x1000,V&0x1F); break;
                 case 3: DRegs[3]=V&0x3F;toot();setchr1(0x1400,V&0x1F); break;
                 case 4: DRegs[4]=V&0x3F;toot();setchr1(0x1800,V&0x1F); break;
                 case 5: DRegs[5]=V&0x3F;toot();setchr1(0x1C00,V&0x1F); break;
                 case 6: DRegs[6]=V&0x3F;
                         setprg8(0x8000,V);
                         break;
                 case 7: DRegs[7]=V&0x3F;
                         setprg8(0xA000,V);
                         break;
                }
                break;
  }
}

static void DBSync()
{
 int x;

 setchr2(0x0000,DRegs[0]);
 setchr2(0x0800,DRegs[1]);

 for (x=0;x<4;x++)
  setchr1(0x1000+x*0x400,DRegs[2+x]);

 setprg8(0x8000,DRegs[6]);
 setprg8(0xa000,DRegs[7]);
 toot();
}

static void DBPower(void)
{
 memset(DRegs,0x3F,8);
 DRegs[0]=DRegs[1]=0x1F;

 DBSync();

 setprg8(0xc000,0x3E);
 setprg8(0xe000,0x3F);

 SetReadHandler(0x8000,0xffff,CartBR);
 SetWriteHandler(0x8000,0xffff,Mapper95_write);
}

static void StateRestore(int version)
{
 DBSync();
}

void Mapper95_Init(CartInfo *info)
{
  info->Power=DBPower;
  AddExState(DB_StateRegs, ~0, 0, 0);
  PPU_hook=dragonbust_ppu;
  GameStateRestore=StateRestore;
}

