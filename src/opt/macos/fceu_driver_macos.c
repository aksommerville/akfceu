#include "fceu/types.h"
#include "fceu/git.h"
#include "fceu/debug.h"
#include "fceu/driver.h"

FILE *FCEUD_UTF8fopen(const char *fn, const char *mode) {
  return fopen(fn,mode);
}

void FCEUD_PrintError(char *s) {
  fprintf(stderr,"FCEUD_PrintError: %s\n",s);
}

void FCEUD_Message(char *s) {
  fprintf(stderr,"%s",s);
}

int FCEUD_SendData(void *data, uint32_t len) {
  fprintf(stderr,"FCEUD_SendData(%d)\n",len);
  return len;
}

int FCEUD_RecvData(void *data, uint32_t len) {
  fprintf(stderr,"FCEUD_Recvdata(%d)\n",len);
  return 0;
}

void FCEUD_NetplayText(uint8_t *text) {
  fprintf(stderr,"FCEUD_NetplayText: %s\n",text);
}

void FCEUD_NetworkClose(void) {
}
