#include "fceu/types.h"
#include "fceu/git.h"
#include "fceu/debug.h"
#include "fceu/driver.h"

static uint8_t fdm_palette[768];

FILE *FCEUD_UTF8fopen(const char *fn, const char *mode) {
  return fopen(fn,mode);
}

void FCEUD_SetPalette(uint8 index, uint8 r, uint8 g, uint8 b) {
  int p=index*3;
  fdm_palette[p++]=r;
  fdm_palette[p++]=g;
  fdm_palette[p]=b;
}

void FCEUD_GetPalette(uint8 i,uint8 *r, uint8 *g, uint8 *b) {
  int p=i*3;
  *r=fdm_palette[p++];
  *g=fdm_palette[p++];
  *b=fdm_palette[p];
}

void FCEUD_PrintError(char *s) {
  fprintf(stderr,"FCEUD_PrintError: %s\n",s);
}

void FCEUD_Message(char *s) {
  fprintf(stderr,"FCEUD_Message: %s\n",s);
}

int FCEUD_SendData(void *data, uint32 len) {
  fprintf(stderr,"FCEUD_SendData(%d)\n",len);
  return len;
}

int FCEUD_RecvData(void *data, uint32 len) {
  fprintf(stderr,"FCEUD_Recvdata(%d)\n",len);
  return 0;
}

void FCEUD_NetplayText(uint8 *text) {
  fprintf(stderr,"FCEUD_NetplayText: %s\n",text);
}

void FCEUD_NetworkClose(void) {
}
