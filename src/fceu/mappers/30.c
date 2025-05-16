/* 30.c
 * AK Sommerville, 2025-02-02
 * Attempting to implement UNROM-512, the default mapper for GBDK.
 */

#include "mapinc.h"
#include <stdlib.h>

static uint8_t *m30_storage=0;

DECLFW(Mapper30_write)
{
  fprintf(stderr,"%s %04x = %02x\n",__func__,A,V);;
}

void Mapper30_init(void)
{
  fprintf(stderr,"*** %s ***\n",__func__);
  if (!m30_storage) m30_storage=malloc(32*16384+4*8192);
  int i=0; for (;i<32;i++) {
    SetupCartPRGMapping(i,m30_storage+i*16384,16384,0);
  }
  for (i=0;i<4;i++) {
    SetupCartCHRMapping(i,m30_storage+32*16384+i*8192,8192,1);
  }
  SetWriteHandler(0x0000,0xffff,Mapper30_write);
}

