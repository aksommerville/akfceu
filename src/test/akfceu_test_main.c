#include "test/akfceu_test.h"
#include "test/akfceu_test_contents.h"
#include <stdint.h>

void FCEUD_SetPalette(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
}

void FCEUD_GetPalette(uint8_t i,uint8_t *r, uint8_t *g, uint8_t *b) {
  *r=*g=*b=0;
}

FILE *FCEUD_UTF8fopen(const char *fn, const char *mode) {
  return fopen(fn,mode);
}

void FCEUD_PrintError(char *s) {
  fprintf(stderr,"FCEUD_PrintError: %s\n",s);
}

void FCEUD_Message(char *s) {
  fprintf(stderr,"%s",s);
}

int main(int argc,char **argv) {
  int failc=0,passc=0;
  int testc=sizeof(akfceu_testv)/sizeof(struct akfceu_test);
  const struct akfceu_test *test=akfceu_testv;
  int i; for (i=0;i<testc;i++,test++) {
    int result=test->fn();
    if (result<0) {
      printf("FAIL: %s\n",test->name);
      failc++;
    } else {
      passc++;
    }
  }
  if (failc) printf("\x1b[41m    \x1b[0m %d fail, %d pass\n",failc,passc);
  else if (passc) printf("\x1b[42m    \x1b[0m 0 fail, %d pass\n",passc);
  else printf("\x1b[40m    \x1b[0m 0 fail, 0 pass\n");
  return 0;
}
