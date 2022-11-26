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
 */
 
void aks_hack_update() {
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
