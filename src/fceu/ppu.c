/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 1998 BERO
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

#include "types.h"
#include "x6502.h"
#include "fceu.h"
#include "ppu.h"
#include "sound.h"
#include "general.h"
#include "endian.h"
#include "memory.h"
#include "cart.h"
#include "palette.h"
#include "state.h"
#include "input.h" 

#define VBlankON        (PPU[0]&0x80)   /* Generate VBlank NMI */
#define Sprite16        (PPU[0]&0x20)   /* Sprites 8x16/8x8        */
#define BGAdrHI         (PPU[0]&0x10)   /* BG pattern adr $0000/$1000 */
#define SpAdrHI         (PPU[0]&0x08)   /* Sprite pattern adr $0000/$1000 */
#define INC32           (PPU[0]&0x04)   /* auto increment 1/32  */

#define SpriteON        (PPU[1]&0x10)   /* Show Sprite             */
#define ScreenON        (PPU[1]&0x08)   /* Show screen             */

#define PPU_status      (PPU[2])

#define Pal     (PALRAM)  

static void FetchSpriteData(void);
static void FASTAPASS(1) RefreshLine(int lastpixel);
static void RefreshSprites(void);
static void CopySprites(uint8_t *target);

static void Fixit1(void);
static uint32_t ppulut1[256];
static uint32_t ppulut2[256];
static uint32_t ppulut3[128];

static void makeppulut(void)
{
 int x;
 int y;

 for (x=0;x<256;x++)
 {
  ppulut1[x]=0;
  for (y=0;y<8;y++)
   ppulut1[x]|=((x>>(7-y))&1)<<(y*4);
  ppulut2[x]=ppulut1[x]<<1;
 }

 {

  int cc,xo,pixel;

  for (cc=0;cc<16;cc++)
  {
   for (xo=0;xo<8;xo++)
   {
    ppulut3[xo|(cc<<3)]=0;
    for (pixel=0;pixel<8;pixel++)
    {
     int shiftr;
      shiftr=(pixel+xo)/8;
      shiftr*=2;
      ppulut3[xo|(cc<<3)]|=(( cc>>shiftr )&3)<<(2+pixel*4);
    }
//    fprintf(stderr,"%08x\n",ppulut3[xo|(cc<<3)]);
   }
  }

 }
}
 
static int ppudead;
static int kook = 0;
int fceuindbg=0;

int MMC5Hack;
uint32_t MMC5HackVROMMask;
uint8_t *MMC5HackExNTARAMPtr;
uint8_t *MMC5HackVROMPTR;
uint8_t MMC5HackCHRMode=0;
uint8_t MMC5HackSPMode;  
uint8_t MMC5HackSPScroll;
uint8_t MMC5HackSPPage;  


uint8_t VRAMBuffer,PPUGenLatch;
uint8_t *vnapage[4];
uint8_t PPUNTARAM; 
uint8_t PPUCHRRAM; 

/* Color deemphasis emulation.  Joy... */
static uint8_t deemp=0;
static int deempcnt[8];

void (*GameHBIRQHook)(void), (*GameHBIRQHook2)(void);
void FP_FASTAPASS(1) (*PPU_hook)(uint32_t A);

uint8_t vtoggle=0;
uint8_t XOffset=0;
  
uint32_t TempAddr,RefreshAddr;
 
static int maxsprites=8; 
   
/* scanline is equal to the current visible scanline we're on. */
    
int scanline;
static uint32_t scanlines_per_frame;
   
uint8_t PPU[4];
uint8_t PPUSPL;
uint8_t NTARAM[0x800],PALRAM[0x20],SPRAM[0x100],SPRBUF[0x100];


#define MMC5SPRVRAMADR(V)      &MMC5SPRVPage[(V)>>10][(V)]
#define MMC5BGVRAMADR(V)      &MMC5BGVPage[(V)>>10][(V)] 
#define VRAMADR(V)      &VPage[(V)>>10][(V)]


static DECLFR(A2002)
{
      uint8_t ret;
       
      FCEUPPU_LineUpdate();
      ret = PPU_status;
      ret|=PPUGenLatch&0x1F;

      #ifdef FCEUDEF_DEBUGGER
      if (!fceuindbg)
      #endif
      {
        vtoggle=0;
        PPU_status&=0x7F;
        PPUGenLatch=ret;
      }
      return ret;
}

static DECLFR(A200x)  /* Not correct for $2004 reads. */
{
      FCEUPPU_LineUpdate();
      return PPUGenLatch;
}

/*
static DECLFR(A2004)
{
      uint8_t ret;

      FCEUPPU_LineUpdate();
      ret = SPRAM[PPU[3]];

                  if (PPUSPL>=8)
                  {
                   if (PPU[3]>=8)
                    ret = SPRAM[PPU[3]];
                  }
                  else
                  {
                   //fprintf(stderr,"$%02x:$%02x\n",PPUSPL,V);
                   ret = SPRAM[PPUSPL];
                  }
                  PPU[3]++;
                  PPUSPL++;
      PPUGenLatch = ret;
      fprintf(stderr,"%d, %02x\n",scanline,ret);
      return(ret);
}
*/
static DECLFR(A2007)
{
      uint8_t ret;
      uint32_t tmp=RefreshAddr&0x3FFF;

      FCEUPPU_LineUpdate();
                       
      ret=VRAMBuffer;

      #ifdef FCEUDEF_DEBUGGER
      if (!fceuindbg)
      #endif
      {
        if (PPU_hook) PPU_hook(tmp);
        PPUGenLatch=VRAMBuffer;
        if (tmp<0x2000) {
          VRAMBuffer=VPage[tmp>>10][tmp];
        } else {  
          VRAMBuffer=vnapage[(tmp>>10)&0x3][tmp&0x3FF];
        }
      }
      #ifdef FCEUDEF_DEBUGGER
      if (!fceuindbg)
      #endif
      {
        if (INC32) RefreshAddr+=32;
        else RefreshAddr++;
        if (PPU_hook) PPU_hook(RefreshAddr&0x3fff);
      }
      return ret;
}

static DECLFW(B2000)
{
    //fprintf(stderr,"%04x:$%02x, %d\n",A,V&0x38,scanline);

    FCEUPPU_LineUpdate();
    PPUGenLatch=V;
    if (!(PPU[0]&0x80) && (V&0x80) && (PPU_status&0x80)) {
      //fprintf(stderr,"Trigger NMI, %d, %d\n",timestamp,ppudead);
      TriggerNMI2();
    }
    PPU[0]=V;
    TempAddr&=0xF3FF;
    TempAddr|=(V&3)<<10;
}

static DECLFW(B2001)
{
    //fprintf(stderr,"%04x:$%02x, %d\n",A,V,scanline);
    FCEUPPU_LineUpdate();
    PPUGenLatch=V;
    PPU[1]=V;
    if (V&0xE0) deemp=V>>5;
}

static DECLFW(B2002) {
  PPUGenLatch=V;
}

static DECLFW(B2003) {
  //fprintf(stderr,"$%04x:$%02x, %d, %d\n",A,V,timestamp,scanline);
  PPUGenLatch=V;
  PPU[3]=V;
  PPUSPL=V&0x7;
}

static DECLFW(B2004)
{
    //fprintf(stderr,"Wr: %04x:$%02x\n",A,V);

                PPUGenLatch=V;
                if (PPUSPL>=8)
                {
                 if (PPU[3]>=8)
                  SPRAM[PPU[3]]=V;
                }
                else
                {  
                 //fprintf(stderr,"$%02x:$%02x\n",PPUSPL,V);
                 SPRAM[PPUSPL]=V;
                }
                PPU[3]++;
                PPUSPL++;

}

static DECLFW(B2005)
{
    uint32_t tmp=TempAddr;
    FCEUPPU_LineUpdate();
    PPUGenLatch=V;
    if (!vtoggle) {
      tmp&=0xFFE0;
      tmp|=V>>3; 
      XOffset=V&7;
    } else {  
      tmp&=0x8C1F;
      tmp|=((V&~0x7)<<2);
      tmp|=(V&7)<<12;
    }
    TempAddr=tmp;
    vtoggle^=1; 
}


static DECLFW(B2006)
{
    FCEUPPU_LineUpdate();

    PPUGenLatch=V;
    if (!vtoggle) {
      TempAddr&=0x00FF;
      TempAddr|=(V&0x3f)<<8;
    } else {  
      TempAddr&=0xFF00;
      TempAddr|=V;
      RefreshAddr=TempAddr;
      if (PPU_hook) PPU_hook(RefreshAddr);
      //fprintf(stderr,"%d, %04x\n",scanline,RefreshAddr);
    }
    vtoggle^=1;
}

static DECLFW(B2007)
{
                        uint32_t tmp=RefreshAddr&0x3FFF;
                        PPUGenLatch=V;
                        if (tmp>=0x3F00)
                        {
                         // hmmm....
                         if (!(tmp&0xf))
                          PALRAM[0x00]=PALRAM[0x04]=PALRAM[0x08]=PALRAM[0x0C]=V&0x3F;
                         else if (tmp&3) PALRAM[(tmp&0x1f)]=V&0x3f;
                        }
                        else if (tmp<0x2000)
                        {
                          if (PPUCHRRAM&(1<<(tmp>>10)))
                            VPage[tmp>>10][tmp]=V;
                        }  
                        else
                        {
                         if (PPUNTARAM&(1<<((tmp&0xF00)>>10)))
                          vnapage[((tmp&0xF00)>>10)][tmp&0x3FF]=V;
                        }
                        if (INC32) RefreshAddr+=32;
                        else RefreshAddr++;
                        if (PPU_hook) PPU_hook(RefreshAddr&0x3fff);
}

static DECLFW(B4014)
{
        uint32_t t=V<<8;
        int x;

        for (x=0;x<256;x++)
         X6502_DMW(0x2004,X6502_DMR(t+x));
}

#define PAL(c)  ((c)+cc)

#define GETLASTPIXEL    (PAL?((timestamp*48-linestartts)/15) : ((timestamp*48-linestartts)>>4) )

static uint8_t *Pline,*Plinef;
static int firsttile; 
static int linestartts;
static int tofix=0;

static void ResetRL(uint8_t *target)
{
 memset(target,0xFF,256);
 if (InputScanlineHook)
  InputScanlineHook(0,0,0,0);
 Plinef=target;
 Pline=target;
 firsttile=0;
 linestartts=timestamp*48+X.count;
 tofix=0;
 FCEUPPU_LineUpdate();
 tofix=1; 
}
static uint8_t sprlinebuf[256+8];

void FCEUPPU_LineUpdate(void)
{
 #ifdef FCEUDEF_DEBUGGER
 if (!fceuindbg)
 #endif
  if (Pline)
  {
   int l=GETLASTPIXEL;
   RefreshLine(l);
  }
}
 
static int tileview=0;
static int rendis = 0;

void FCEUI_ToggleTileView(void)
{
 tileview^=1;
}

void FCEUI_SetRenderDisable(int sprites, int bg)
{
 //fprintf(stderr,"%d, %d\n",sprites,bg);
 if (sprites >= 0)
 {
  if (sprites == 2) rendis ^= 1;
  else rendis = (rendis &~1) | sprites?1:0;
 }
 if (bg >= 0)
 {
  if (bg == 2) rendis ^= 2;
  else rendis = (rendis &~2) | bg?2:0;
 }
}
/*
static void TileView(void)
{
 uint8_t *P=XBuf+16*256;
 int bgh;
 int y; 
 int X1;
 for (bgh=0;bgh<2;bgh++)
 for (y=0;y<16*8;y++)  
 for (P=XBuf+bgh*128+(16+y)*256,X1=16;X1;X1--,P+=8)
 {
  uint8_t *C;
  register uint8_t cc;
  uint32_t vadr;
 
  vadr=((((16-X1)|((y>>3)<<4))<<4)|(y&7))+bgh*0x1000;
  //C= ROM+vadr+turt*8192;
  C = VRAMADR(vadr);
  //if((vadr+turt*8192)>=524288)
  //fprintf(stderr,"%d ",vadr+turt*8192);
  cc=0;
  //#include "pputile.h"
 }
}
*/
static void CheckSpriteHit(int p);

static void EndRL(void)
{
 RefreshLine(272);
 if (tofix)
  Fixit1();
 CheckSpriteHit(272);
 Pline=0;
}

static int32_t sphitx;
static uint8_t sphitdata;

static void CheckSpriteHit(int p)
{
 int l=p-16;
 int x;

 if (sphitx==0x100) return;

 for (x=sphitx;x<(sphitx+8) && x<l;x++)
 {
   if ((sphitdata&(0x80>>(x-sphitx))) && !(Plinef[x]&64))
   {
    PPU_status|=0x40;
    //fprintf(stderr,"Ha:  %d, %d, Hita: %d, %d, %d, %d, %d\n",p,p&~7,scanline,GETLASTPIXEL-16,&Plinef[x],Pline,Pline-Plinef);
    //fprintf(stderr,"%d\n",GETLASTPIXEL-16);
    //if(Plinef[x] == 0xFF)
    //fprintf(stderr,"PL: %d, %02x\n",scanline, Plinef[x]);
    sphitx=0x100;
    break;
   }
 }
}  
static int spork=0;     /* spork the world.  Any sprites on this line?
                           Then this will be set to 1.  Needed for zapper
                           emulation and *gasp* sprite emulation.
                        */
                         
// lasttile is really "second to last tile."
static void FASTAPASS(1) RefreshLine(int lastpixel)
{
        static uint32_t pshift[2];
        static uint32_t atlatch;
        uint32_t smorkus=RefreshAddr;

        #define RefreshAddr smorkus
        uint32_t vofs;
        int X1;

        register uint8_t *P=Pline;
        int lasttile=lastpixel>>3;
        int numtiles;
        static int norecurse=0; /* Yeah, recursion would be bad.
                                    PPU_hook() functions can call
                                    mirroring/chr bank switching functions,
                                    which call FCEUPPU_LineUpdate, which call this
                                    function. */
        if (norecurse) return;

  if (sphitx != 0x100 && !(PPU_status&0x40))
        {
   if ((sphitx < (lastpixel-16)) && !(sphitx < ((lasttile - 2)*8)))
   {
    //fprintf(stderr,"OK: %d\n",scanline);
    lasttile++;
   }

        }

        if (lasttile>34) lasttile=34;
        numtiles=lasttile-firsttile;
       
  if (numtiles<=0) return;

        P=Pline;
       
        vofs=0;
       
        vofs=((PPU[0]&0x10)<<8) | ((RefreshAddr>>12)&7);
       
        if (!ScreenON && !SpriteON)
        {
         uint32_t tem;
         tem=Pal[0]|(Pal[0]<<8)|(Pal[0]<<16)|(Pal[0]<<24);
         tem|=0x40404040;
         FCEU_dwmemset(Pline,tem,numtiles*8);
         P+=numtiles*8;
         Pline=P;
        
         firsttile=lasttile;

         #define TOFIXNUM (272-0x4)
         if (lastpixel>=TOFIXNUM && tofix)
         {
          Fixit1();
          tofix=0;
         }

         if (InputScanlineHook && (lastpixel-16)>=0)
         {
          InputScanlineHook(Plinef,spork?sprlinebuf:0,linestartts,lasttile*8-16);
         }
         return;
        }

  /* Priority bits, needed for sprite emulation. */
        Pal[0]|=64;
        Pal[4]|=64;
        Pal[8]|=64;
        Pal[0xC]|=64;

        /* This high-level graphics MMC5 emulation code was written
           for MMC5 carts in "CL" mode.  It's probably not totally
           correct for carts in "SL" mode.
        */     

  #define PPUT_MMC5
        if (MMC5Hack)
        {
         if (MMC5HackCHRMode==0 && (MMC5HackSPMode&0x80))
         {
    int tochange=MMC5HackSPMode&0x1F;
    tochange-=firsttile;
          for (X1=firsttile;X1<lasttile;X1++)
          {
           if ((tochange<=0 && MMC5HackSPMode&0x40) || (tochange>0 && !(MMC5HackSPMode&0x40)))
           {
            #define PPUT_MMC5SP
            #include "pputile.h"
       #undef PPUT_MMC5SP
     }
     else
     {
      #include "pputile.h"     
     }
     tochange--;
    }
   }
         else if (MMC5HackCHRMode==1 && (MMC5HackSPMode&0x80))
   {
          int tochange=MMC5HackSPMode&0x1F;
          tochange-=firsttile;

          #define PPUT_MMC5SP
    #define PPUT_MMC5CHR1
          for (X1=firsttile;X1<lasttile;X1++)
          {
           #include "pputile.h"
          }
    #undef PPUT_MMC5CHR1
          #undef PPUT_MMC5SP
   }
         else if (MMC5HackCHRMode==1)
   {
          #define PPUT_MMC5CHR1
          for (X1=firsttile;X1<lasttile;X1++)
          {
           #include "pputile.h"
          }
          #undef PPUT_MMC5CHR1
   }
         else
   {
    for (X1=firsttile;X1<lasttile;X1++)
    {
     #include "pputile.h"
    }
   }
  }
  #undef PPUT_MMC5
  else if (PPU_hook)
  {
   norecurse=1;
   #define PPUT_HOOK
         for (X1=firsttile;X1<lasttile;X1++)
         {
          #include "pputile.h"
         }
   #undef PPUT_HOOK
   norecurse=0;
  }
  else
  {
         for (X1=firsttile;X1<lasttile;X1++)
         {
    #include "pputile.h"
         }
  }

        #undef vofs
        #undef RefreshAddr

  /* Reverse changes made before. */
        Pal[0]&=63;
        Pal[4]&=63;
        Pal[8]&=63;
        Pal[0xC]&=63;

        RefreshAddr=smorkus;
        if (firsttile<=2 && 2<lasttile && !(PPU[1]&2))
        {
         uint32_t tem;
         tem=Pal[0]|(Pal[0]<<8)|(Pal[0]<<16)|(Pal[0]<<24); 
         tem|=0x40404040;
         *(uint32_t *)Plinef=*(uint32_t *)(Plinef+4)=tem;
        }

  if (!ScreenON)
  {
         uint32_t tem;
         int tstart,tcount;
         tem=Pal[0]|(Pal[0]<<8)|(Pal[0]<<16)|(Pal[0]<<24);
         tem|=0x40404040;
         
         tcount=lasttile-firsttile;
         tstart=firsttile-2;
         if (tstart<0)
         {
          tcount+=tstart;
          tstart=0;
         }
         if (tcount>0)
          FCEU_dwmemset(Plinef+tstart*8,tem,tcount*8);
  }

        if (lastpixel>=TOFIXNUM && tofix)
        {
    //puts("Fixed");
          Fixit1();
          tofix=0;
        }

        //CheckSpriteHit(lasttile*8); //lasttile*8); //lastpixel);
 
  CheckSpriteHit(lastpixel);  /* This only works right because
             of a hack earlier in this function.
          */
        if (InputScanlineHook && (lastpixel-16)>=0)
        {
         InputScanlineHook(Plinef,spork?sprlinebuf:0,linestartts,lasttile*8-16);
        }
        Pline=P;
        firsttile=lasttile;
}

static INLINE void Fixit2(void)
{
   if (ScreenON || SpriteON)
   {
    uint32_t rad=RefreshAddr;
    rad&=0xFBE0;
    rad|=TempAddr&0x041f;
    RefreshAddr=rad;
    //PPU_hook(RefreshAddr);
    //PPU_hook(RefreshAddr,-1);
   }
}

static void Fixit1(void)
{
   if (ScreenON || SpriteON)
   {
    uint32_t rad=RefreshAddr;

    if ((rad&0x7000)==0x7000)
    {
     rad^=0x7000;
     if ((rad&0x3E0)==0x3A0)
     {
      rad^=0x3A0;
      rad^=0x800;
     }
     else
     {
      if ((rad&0x3E0)==0x3e0)
       rad^=0x3e0;
      else rad+=0x20;
     }
    }
    else
     rad+=0x1000;
    RefreshAddr=rad;
    //PPU_hook(RefreshAddr); //,-1);
   }
}

void MMC5_hb(int);     /* Ugh ugh ugh. */
static void DoLine(void)
{
 int x;
 uint8_t *target=XBuf+(scanline<<8);

 if (MMC5Hack && (ScreenON || SpriteON)) MMC5_hb(scanline);

 X6502_Run(256);
 EndRL();

 if (rendis & 2)  /* User asked to not display background data. */
 {
  uint32_t tem;
  tem=Pal[0]|(Pal[0]<<8)|(Pal[0]<<16)|(Pal[0]<<24);
  tem|=0x40404040;
  FCEU_dwmemset(target,tem,256);
 }

 if (SpriteON)
  CopySprites(target);

 if (ScreenON || SpriteON)        // Yes, very el-cheapo.
 {
  if (PPU[1]&0x01)
  {
   for (x=63;x>=0;x--)
   *(uint32_t *)&target[x<<2]=(*(uint32_t*)&target[x<<2])&0x30303030;
  }
 }
 if ((PPU[1]>>5)==0x7)
 {
  for (x=63;x>=0;x--)
   *(uint32_t *)&target[x<<2]=((*(uint32_t*)&target[x<<2])&0x3f3f3f3f)|0xc0c0c0c0;
 }
 else if (PPU[1]&0xE0)
  for (x=63;x>=0;x--)
   *(uint32_t *)&target[x<<2]=(*(uint32_t*)&target[x<<2])|0x40404040;
 else
  for (x=63;x>=0;x--)
   *(uint32_t *)&target[x<<2]=((*(uint32_t*)&target[x<<2])&0x3f3f3f3f)|0x80808080;

 sphitx=0x100;

 if (ScreenON || SpriteON)
  FetchSpriteData();

 if (GameHBIRQHook && (ScreenON || SpriteON) && ((PPU[0]&0x38)!=0x18))
 {
  X6502_Run(6);
  Fixit2();
  X6502_Run(4);
  GameHBIRQHook();
  X6502_Run(85-16-10);
 }
 else
 {
  X6502_Run(6);        // Tried 65, caused problems with Slalom(maybe others)
  Fixit2();
  X6502_Run(85-6-16);

  // A semi-hack for Star Trek: 25th Anniversary
  if (GameHBIRQHook && (ScreenON || SpriteON) && ((PPU[0]&0x38)!=0x18))
   GameHBIRQHook();
 }

 if (SpriteON)
  RefreshSprites();
 if (GameHBIRQHook2 && (ScreenON || SpriteON))
  GameHBIRQHook2();
 scanline++;
 if (scanline<240)
 {
  ResetRL(XBuf+(scanline<<8));
 }
 X6502_Run(16);
}

#define V_FLIP  0x80
#define H_FLIP  0x40
#define SP_BACK 0x20

typedef struct {
        uint8_t y,no,atr,x;
} SPR;

typedef struct {
        uint8_t ca[2],atr,x;
} SPRB;

void FCEUI_DisableSpriteLimitation(int a)
{
 maxsprites=a?64:8;
}

static uint8_t numsprites,SpriteBlurp;
static void FetchSpriteData(void)
{
        uint8_t ns,sb;
        SPR *spr;
        uint8_t H;
  int n;
  int vofs;
        uint8_t P0=PPU[0];

        spr=(SPR *)SPRAM;
        H=8;

        ns=sb=0;

        vofs=(unsigned int)(P0&0x8&(((P0&0x20)^0x20)>>2))<<9;
        H+=(P0&0x20)>>2;

        if (!PPU_hook)
         for (n=63;n>=0;n--,spr++)
         {
                if ((unsigned int)(scanline-spr->y)>=H) continue;
                //fprintf(stderr,"%d, %u\n",scanline,(unsigned int)(scanline-spr->y));
                if (ns<maxsprites)
                {
                 if (n==63) sb=1;

                 {
                  SPRB dst;
                  uint8_t *C;
                  int t;
                  unsigned int vadr;

                  t = (int)scanline-(spr->y);

                  if (Sprite16)
                   vadr = ((spr->no&1)<<12) + ((spr->no&0xFE)<<4);
                  else
                   vadr = (spr->no<<4)+vofs;

                  if (spr->atr&V_FLIP)
                  {
                        vadr+=7;
                        vadr-=t;
                        vadr+=(P0&0x20)>>1;
                        vadr-=t&8;
                  }
                  else
                  {
                        vadr+=t;
                        vadr+=t&8;
                  }

                  if (MMC5Hack) C = MMC5SPRVRAMADR(vadr);
                  else C = VRAMADR(vadr);

                 
                  dst.ca[0]=C[0];
                  dst.ca[1]=C[8];
                  dst.x=spr->x;
                  dst.atr=spr->atr;

                  *(uint32_t *)&SPRBUF[ns<<2]=*(uint32_t *)&dst;
                 }

                 ns++;
                }
                else
                {
                  PPU_status|=0x20;
                  break;
                }
         }
        else
         for (n=63;n>=0;n--,spr++)
         {
                if ((unsigned int)(scanline-spr->y)>=H) continue;

                if (ns<maxsprites)
                {
                 if (n==63) sb=1;

                 {
                  SPRB dst;
                  uint8_t *C;
                  int t;
                  unsigned int vadr;

                  t = (int)scanline-(spr->y);

                  if (Sprite16)
                   vadr = ((spr->no&1)<<12) + ((spr->no&0xFE)<<4);
                  else
                   vadr = (spr->no<<4)+vofs;

                  if (spr->atr&V_FLIP)
                  {
                        vadr+=7;
                        vadr-=t;
                        vadr+=(P0&0x20)>>1;
                        vadr-=t&8;
                  }
                  else
                  {
                        vadr+=t;
                        vadr+=t&8;
                  }

                  if (MMC5Hack) C = MMC5SPRVRAMADR(vadr);
                  else C = VRAMADR(vadr);
                  dst.ca[0]=C[0];
      if (ns<8)
      {
       PPU_hook(0x2000);
                   PPU_hook(vadr);
      }
                  dst.ca[1]=C[8];
                  dst.x=spr->x;
                  dst.atr=spr->atr;


                  *(uint32_t *)&SPRBUF[ns<<2]=*(uint32_t *)&dst;
                 }

                 ns++;
                }
                else
                {
                  PPU_status|=0x20;
                  break;
                }
         }
        //if(ns>=7)
        //fprintf(stderr,"%d %d\n",scanline,ns);
        if (ns>8) PPU_status|=0x20;  /* Handle case when >8 sprites per
             scanline option is enabled. */
  else if (PPU_hook)
  {
   for (n=0;n<(8-ns);n++)
   {
                 PPU_hook(0x2000);
                 PPU_hook(vofs);
   }
  }
        numsprites=ns;
        SpriteBlurp=sb;
}

static void RefreshSprites(void)
{
        int n;
        SPRB *spr;

        spork=0;
        if (!numsprites) return;

        FCEU_dwmemset(sprlinebuf,0x80808080,256);
        numsprites--;
  spr = (SPRB*)SPRBUF+numsprites;

  for (n=numsprites;n>=0;n--,spr--)
  {
   //#ifdef C80x86
   //register uint32_t pixdata asm ("eax");
   //register uint8_t J, atr;
   //#else
   register uint32_t pixdata;
   register uint8_t J,atr;
   //#endif

   int x=spr->x;
         uint8_t *C;
         uint8_t *VB;
               
         pixdata=ppulut1[spr->ca[0]]|ppulut2[spr->ca[1]];
         J=spr->ca[0]|spr->ca[1];
         atr=spr->atr;

                       if (J)
                       {
                        if (n==0 && SpriteBlurp && !(PPU_status&0x40))
                        {
                         sphitx=x;
                         sphitdata=J;
                         if (atr&H_FLIP)
                          sphitdata=    ((J<<7)&0x80) |
                                        ((J<<5)&0x40) |
                                        ((J<<3)&0x20) |
                                        ((J<<1)&0x10) |
                                        ((J>>1)&0x08) |
                                        ((J>>3)&0x04) |
                                        ((J>>5)&0x02) |
                                        ((J>>7)&0x01);                                         
                        }

         C = sprlinebuf+x;
         VB = (PALRAM+0x10)+((atr&3)<<2);

         if (atr&SP_BACK)
         {
          if (atr&H_FLIP)
          {
           if (J&0x80) C[7]=VB[pixdata&3]|0x40;
           pixdata>>=4;
           if (J&0x40) C[6]=VB[pixdata&3]|0x40;
           pixdata>>=4;
           if (J&0x20) C[5]=VB[pixdata&3]|0x40;
           pixdata>>=4;
           if (J&0x10) C[4]=VB[pixdata&3]|0x40;
           pixdata>>=4;
           if (J&0x08) C[3]=VB[pixdata&3]|0x40;
           pixdata>>=4;
           if (J&0x04) C[2]=VB[pixdata&3]|0x40;
           pixdata>>=4;
           if (J&0x02) C[1]=VB[pixdata&3]|0x40;
           pixdata>>=4;
           if (J&0x01) C[0]=VB[pixdata]|0x40;
          } else  {
           if (J&0x80) C[0]=VB[pixdata&3]|0x40;  
           pixdata>>=4;
           if (J&0x40) C[1]=VB[pixdata&3]|0x40;  
           pixdata>>=4;
           if (J&0x20) C[2]=VB[pixdata&3]|0x40;
           pixdata>>=4;
           if (J&0x10) C[3]=VB[pixdata&3]|0x40;
           pixdata>>=4;
           if (J&0x08) C[4]=VB[pixdata&3]|0x40;
           pixdata>>=4;
           if (J&0x04) C[5]=VB[pixdata&3]|0x40;
           pixdata>>=4;
           if (J&0x02) C[6]=VB[pixdata&3]|0x40;
           pixdata>>=4;
           if (J&0x01) C[7]=VB[pixdata]|0x40;
          }
         } else {
          if (atr&H_FLIP)
          {
           if (J&0x80) C[7]=VB[pixdata&3];  
           pixdata>>=4;
           if (J&0x40) C[6]=VB[pixdata&3];  
           pixdata>>=4;
           if (J&0x20) C[5]=VB[pixdata&3];
           pixdata>>=4;
           if (J&0x10) C[4]=VB[pixdata&3];
           pixdata>>=4;
           if (J&0x08) C[3]=VB[pixdata&3];
           pixdata>>=4;
           if (J&0x04) C[2]=VB[pixdata&3];
           pixdata>>=4;
           if (J&0x02) C[1]=VB[pixdata&3];
           pixdata>>=4;
           if (J&0x01) C[0]=VB[pixdata];
          }else{                
           if (J&0x80) C[0]=VB[pixdata&3];  
           pixdata>>=4;
           if (J&0x40) C[1]=VB[pixdata&3];  
           pixdata>>=4;
           if (J&0x20) C[2]=VB[pixdata&3];
           pixdata>>=4;
           if (J&0x10) C[3]=VB[pixdata&3];
           pixdata>>=4;
           if (J&0x08) C[4]=VB[pixdata&3];
           pixdata>>=4;
           if (J&0x04) C[5]=VB[pixdata&3];
           pixdata>>=4;
           if (J&0x02) C[6]=VB[pixdata&3];
           pixdata>>=4;
           if (J&0x01) C[7]=VB[pixdata];
          }
         }
        }
      }
     SpriteBlurp=0;
     spork=1;
}

static void CopySprites(uint8_t *target)
{
      uint8_t n=((PPU[1]&4)^4)<<1;
      uint8_t *P=target;

      if (!spork) return;
      spork=0;

      if (rendis & 1) return;  /* User asked to not display sprites. */

      loopskie:
      {
       uint32_t t=*(uint32_t *)(sprlinebuf+n);

       if (t!=0x80808080)
       {
        #ifdef LSB_FIRST
        if (!(t&0x80))
        {
         if (!(t&0x40) || (P[n]&0x40))       // Normal sprite || behind bg sprite
          P[n]=sprlinebuf[n];
        }

        if (!(t&0x8000))
        {
         if (!(t&0x4000) || (P[n+1]&0x40))       // Normal sprite || behind bg sprite
          P[n+1]=(sprlinebuf+1)[n];
        }

        if (!(t&0x800000))
        {
         if (!(t&0x400000) || (P[n+2]&0x40))       // Normal sprite || behind bg sprite
          P[n+2]=(sprlinebuf+2)[n];
        }

        if (!(t&0x80000000))
        {
         if (!(t&0x40000000) || (P[n+3]&0x40))       // Normal sprite || behind bg sprite
          P[n+3]=(sprlinebuf+3)[n];
        }
        #else
        /* TODO:  Simplify */
        if (!(t&0x80000000))
        {
         if (!(t&0x40000000))       // Normal sprite
          P[n]=sprlinebuf[n];
         else if (P[n]&64)        // behind bg sprite
          P[n]=sprlinebuf[n];
        }

        if (!(t&0x800000))
        {
         if (!(t&0x400000))       // Normal sprite
          P[n+1]=(sprlinebuf+1)[n];
         else if (P[n+1]&64)        // behind bg sprite
          P[n+1]=(sprlinebuf+1)[n];
        }

        if (!(t&0x8000))
        {
         if (!(t&0x4000))       // Normal sprite
          P[n+2]=(sprlinebuf+2)[n];
         else if (P[n+2]&64)        // behind bg sprite
          P[n+2]=(sprlinebuf+2)[n];
        }

        if (!(t&0x80))
        {
         if (!(t&0x40))       // Normal sprite
          P[n+3]=(sprlinebuf+3)[n];
         else if (P[n+3]&64)        // behind bg sprite
          P[n+3]=(sprlinebuf+3)[n];
        }
        #endif
       }
      }
      n+=4;
      if (n) goto loopskie;
}

void FCEUPPU_SetVideoSystem(int w)
{
 if (w)
 {
  scanlines_per_frame=312;
  FSettings.FirstSLine=FSettings.UsrFirstSLine[1];
  FSettings.LastSLine=FSettings.UsrLastSLine[1];
 }
 else
 {
  scanlines_per_frame=262;
  FSettings.FirstSLine=FSettings.UsrFirstSLine[0];
  FSettings.LastSLine=FSettings.UsrLastSLine[0];
 }
}

void FCEUPPU_Init(void)
{
 makeppulut();
}

void FCEUPPU_Reset(void)
{
        VRAMBuffer=PPU[0]=PPU[1]=PPU_status=PPU[3]=0;  
        PPUSPL=0;
        PPUGenLatch=0;
        RefreshAddr=TempAddr=0;
        vtoggle = 0;
        ppudead = 2;
  kook = 0;
}

void FCEUPPU_Power(void)
{
  int x;

        memset(NTARAM,0x00,0x800);
        memset(PALRAM,0x00,0x20);
        memset(SPRAM,0x00,0x100);
        FCEUPPU_Reset();

        for (x=0x2000;x<0x4000;x+=8)
        {
         ARead[x]=A200x;
         BWrite[x]=B2000;
         ARead[x+1]=A200x;
         BWrite[x+1]=B2001;
         ARead[x+2]=A2002;
         BWrite[x+2]=B2002;
         ARead[x+3]=A200x;
         BWrite[x+3]=B2003;
         ARead[x+4]=A200x; //A2004;
         BWrite[x+4]=B2004;
         ARead[x+5]=A200x;
         BWrite[x+5]=B2005;
         ARead[x+6]=A200x;
         BWrite[x+6]=B2006;
         ARead[x+7]=A2007;
         BWrite[x+7]=B2007;
        }
        BWrite[0x4014]=B4014;
}


int FCEUPPU_Loop(int skip)
{
  if (ppudead) /* Needed for Knight Rider, possibly others. */
  {
   memset(XBuf, 0x80, 256*240);
   X6502_Run(scanlines_per_frame*(256+85));
   ppudead--;
  }
  else
  {
   X6502_Run(256+85);
   PPU_status |= 0x80;
   PPU[3]=PPUSPL=0;             /* Not sure if this is correct.  According
                                  to Matt Conte and my own tests, it is.  Timing is probably
                                  off, though.  NOTE:  Not having this here
                                  breaks a Super Donkey Kong game. */
                                /* I need to figure out the true nature and length
                                   of this delay.
                                */
   X6502_Run(12);
   if (VBlankON) TriggerNMI();
   X6502_Run((scanlines_per_frame-242)*(256+85)-12); //-12);
   PPU_status&=0x1f;
   X6502_Run(256);

   {
    int x;

    if (ScreenON || SpriteON)
    {
     if (GameHBIRQHook && ((PPU[0]&0x38)!=0x18))
      GameHBIRQHook();
     if (PPU_hook)
      for (x=0;x<42;x++) {PPU_hook(0x2000); PPU_hook(0);}
     if (GameHBIRQHook2)
      GameHBIRQHook2();
    }
    X6502_Run(85-16);
    if (ScreenON || SpriteON)
    { 
     RefreshAddr=TempAddr; 
     if (PPU_hook) PPU_hook(RefreshAddr&0x3fff); 
    }

    /* Clean this stuff up later. */
    spork=numsprites=0;
    ResetRL(XBuf);

    X6502_Run(16-kook);
    kook ^= 1;
   }
   #ifdef FRAMESKIP
   else if (skip)
   {
    int y;

    y=SPRAM[0];
    y++;

    PPU_status|=0x20;       // Fixes "Bee 52".  Does it break anything?
    if (GameHBIRQHook)
    {
     X6502_Run(256);
     for (scanline=0;scanline<240;scanline++)
     {
      if (ScreenON || SpriteON)
       GameHBIRQHook();
      if (scanline==y && SpriteON) PPU_status|=0x40;
      X6502_Run((scanline==239)?85:(256+85));
     }   
    }
    else if (y<240)
    {
     X6502_Run((256+85)*y);
     if (SpriteON) PPU_status|=0x40; // Quick and very dirty hack.
     X6502_Run((256+85)*(240-y));
    }
    else
     X6502_Run((256+85)*240);
   }
   else
   #endif
   {
    int x,max,maxref;

    deemp=PPU[1]>>5;
    for (scanline=0;scanline<240;)       //scanline is incremented in  DoLine.  Evil. :/
    {
     deempcnt[deemp]++;
     DoLine();
    }
    if (MMC5Hack && (ScreenON || SpriteON)) MMC5_hb(scanline);
    for (x=1,max=0,maxref=0;x<7;x++)
    {
     if (deempcnt[x]>max)
     {
      max=deempcnt[x];
      maxref=x;
     }
     deempcnt[x]=0;
    }
    //FCEU_DispMessage("%2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x %d",deempcnt[0],deempcnt[1],deempcnt[2],deempcnt[3],deempcnt[4],deempcnt[5],deempcnt[6],deempcnt[7],maxref);
    //memset(deempcnt,0,sizeof(deempcnt));
    SetNESDeemph(maxref,0);
   }
  } /* else... to if (ppudead) */

  #ifdef FRAMESKIP  
  if (skip)
  {
   FCEU_PutImageDummy();
   return(0);
  }
  else
  #endif
  {
   return(1);
  }  
}

static uint16_t TempAddrT,RefreshAddrT;

void FCEUPPU_LoadState(int version)
{
          TempAddr=TempAddrT;
          RefreshAddr=RefreshAddrT;
}

SFORMAT FCEUPPU_STATEINFO[]={
  { NTARAM, 0x800, "NTAR"},
  { PALRAM, 0x20, "PRAM"},
  { SPRAM, 0x100, "SPRA"},
  { PPU, 0x4, "PPUR"},
  { &kook, 1, "KOOK"},
  { &ppudead, 1, "DEAD"},
  { &PPUSPL, 1, "PSPL"},
  { &XOffset, 1, "XOFF"},
  { &vtoggle, 1, "VTOG"},
  { &RefreshAddrT, 2|FCEUSTATE_RLSB, "RADD"},
  { &TempAddrT, 2|FCEUSTATE_RLSB, "TADD"},
  { &VRAMBuffer, 1, "VBUF"},
  { &PPUGenLatch, 1, "PGEN"},
  { 0 }
 };

void FCEUPPU_SaveState(void)
{
 TempAddrT=TempAddr;  
 RefreshAddrT=RefreshAddr;
}
