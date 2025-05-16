/* aks_hack.c
 * Scratch code that executes on each video frame, for screwing around.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern uint8_t RAM[0x800];

static uint8_t aks_pvram[0x800]={0};

#define AKS_HACK_INTERVAL 60 /* s/60 */
static int aks_hack_clock=0;

/* Dump RAM.
 */
 
static void dump_ram() {
  fprintf(stderr,"--- RAM dump ---\n");
  const int bytes_per_line=64; // must be factor of sizeof(RAM), ie power of 2
  int p=0;
  for (;p<sizeof(RAM);) {
    fprintf(stderr,"%04x:",p);
    int i=bytes_per_line;
    for (;i-->0;p++) {
      //if (aks_pvram[p]==RAM[p]) fprintf(stderr," .."); else
      fprintf(stderr," %02x",RAM[p]);
    }
    fprintf(stderr,"\n");
  }
  memcpy(aks_pvram,RAM,sizeof(RAM));
}

/* Main entry point.
 * (in1) is the Emuhost-format state of player 1.
 */
 
void aks_hack_update(int in1) {
  if (aks_hack_clock--) return;
  aks_hack_clock=AKS_HACK_INTERVAL;
  
  //dump_ram();
  
  /**
  int p=0x0107,i;
  fprintf(stderr,"0x%04x:",p);
  for (i=66;i-->0;p++) fprintf(stderr," %02x",RAM[p]);
  //p=0x0511;
  //fprintf(stderr," ; 0x%04x:",p);
  //for (i=7;i-->0;p++) fprintf(stderr," %02x",RAM[p]);
  fprintf(stderr,"\n");
  /**/
  
}

/* Test framebuffer: Is this rectangle composed entirely of this one pixel?
 */
 
static int rect_is_solid(const uint8_t *fb,int x,int y,int w,int h,uint8_t ix) {
  if ((x<0)||(y<0)||(x+w>256)||(y+h>240)) return 0;
  fb+=y*256+x;
  for (;h-->0;fb+=256) {
    const uint8_t *p=fb;
    int xi=w;
    for (;xi-->0;p++) {
      if (*p!=ix) {
        return 0;
      }
    }
  }
  return 1;
}

/* Fill rectangle in framebuffer.
 */
 
static void fill_rect(uint8_t *fb,int x,int y,int w,int h,uint8_t ix) {
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x+w>256) w=256-x;
  if (y+h>240) h=240-y;
  if ((w<1)||(h<1)) return;
  uint8_t *dstrow=fb+y*256+x;
  int yi=h;
  for (;yi-->0;dstrow+=256) memset(dstrow,ix,w);
}

/* Also main entry point, when a framebuffer is present.
 * 256x240 of I8, packed.
 * We have the opportunity to modify it before delivery to Emuhost.
 * (in1) is the Emuhost-format state of player 1.
 */
 
static int pvblack=0;
static int pvtime=0;
static int framec=0;
static int lblack=0,rblack=0;
 
void aks_hack_framebuffer(uint8_t *fb,int in1) {
  framec++;
  
  /* Tetris: Vertical timing. Results are recorded in eggsamples/tetris.
   * Top two rows of the Tetris playfield: 96,48,79,16
   * Black=0x8f
   * Signal when it changes from all-black to not-all-black and note frame count since last transition.
   *
  //fill_rect(fb,96,48,79,16,0x8f);
  if (rect_is_solid(fb,96,48,79,16,0x8f)) {
    if (!pvblack) {
      fprintf(stderr,"%s:%d: Top two rows are empty. %d frames since last transition to empty.\n",__FILE__,__LINE__,framec-pvtime);
      pvtime=framec;
      pvblack=1;
    }
  } else {
    pvblack=0;
  }
  /**/
  
  /* Tetris: Horizontal timing. It's 52 frames, and that includes the longer initial step.
   * Signal the interval between left column becoming black and right column becoming not-black.
   * For a 2-column-wide tile, that's 8 steps.
   *
  //fill_rect(fb,96,48,8,160,0x8f);
  //fill_rect(fb,168,48,8,160,0x8f);
  if (rect_is_solid(fb,96,48,8,160,0x8f)) { // left black
    if (!lblack) {
      lblack=1;
      pvtime=framec;
    }
  } else {
    lblack=0;
  }
  if (rect_is_solid(fb,168,48,8,160,0x8f)) { // right black
    if (!rblack) rblack=1;
  } else if (rblack) {
    rblack=0;
    if (pvtime) {
      fprintf(stderr,"Time left to right: %d frames\n",framec-pvtime);
      pvtime=0;
    }
  }
  /**/
}
