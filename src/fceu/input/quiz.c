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

static uint8_t QZVal,QZValR;
static uint8_t FunkyMode;

static uint8_t FP_FASTAPASS(2) QZ_Read(int w, uint8_t ret)
{
 if (w)
 {
  //if(X.PC==0xdc7d) return(0xFF);
  //printf("Blah: %04x\n",X.PC);
  //FCEUI_DumpMem("dmp2",0xc000,0xffff);

  ret|=(QZValR&0x7)<<2;
  QZValR=QZValR>>3;

  if (FunkyMode)
  {
   //ret=0x14;
   //puts("Funky");
   QZValR|=0x28;
  }
  else
  {
   QZValR|=0x38;
  }
 }
 return(ret);
}

static void QZ_Strobe(void)
{
 QZValR=QZVal;
 //puts("Strobe");
}

static void FP_FASTAPASS(1) QZ_Write(uint8_t V)
{
 //printf("Wr: %02x\n",V);
 FunkyMode=V&4;
}

static void FP_FASTAPASS(2) QZ_Update(void *data, int arg)
{
 QZVal=*(uint8_t *)data;
}

static INPUTCFC QuizKing={QZ_Read,QZ_Write,QZ_Strobe,QZ_Update,0,0};

INPUTCFC *FCEU_InitQuizKing(void)
{
 QZVal=QZValR=0;
 return(&QuizKing);
}
