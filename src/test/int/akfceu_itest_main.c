#include "test/akfceu_test.h"
#include "test/akfceu_test_contents.h"

void FCEUD_SetPalette(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {}
void FCEUD_GetPalette(uint8_t i,uint8_t *r, uint8_t *g, uint8_t *b) {}
FILE *FCEUD_UTF8fopen(const char *fn, const char *mode) { return 0; }
void FCEUD_PrintError(char *s) {}
void FCEUD_Message(char *s) {}

/* Is (q) present in this comma-delimited list?
 */
 
static int akfceu_string_in_comma_list(const char *q,const char *list) {
  int qc=0; while (q[qc]) qc++;
  while (*list) {
    if ((unsigned char)*list<=0x20) { list++; continue; }
    if (*list==',') { list++; continue; }
    int subc=0;
    while (list[subc]&&(list[subc]!=',')) subc++;
    const char *next=list+subc;
    while (subc&&((unsigned char)list[subc-1]<=0x20)) subc--;
    if ((subc==qc)&&!memcmp(list,q,subc)) return 1;
    list=next;
  }
  return 0;
}

/* Is this test ignored or filtered? Should we run it?
 */
 
static int akfceu_itest_should_run(const struct akfceu_itest *test,int argc,char **argv) {
  
  // If the test name or any of its groups is provided on the command line, yes.
  int i=1; for (;i<argc;i++) {
    const char *arg=argv[i];
    if (!strcmp(arg,test->name)) return 1;
    if (akfceu_string_in_comma_list(arg,test->groups)) return 1;
  }
  
  // If it's marked "ignore" (ie XXX_AKFCEU_TEST), no.
  if (!test->enable) return 0;
  
  // If a non-matching filter was provided, no.
  if (argc>1) return 0;
  
  // Yes.
  return 1;
}

/* Main.
 */

int main(int argc,char **argv) {
  const struct akfceu_itest *test=akfceu_itestv;
  int i=sizeof(akfceu_itestv)/sizeof(struct akfceu_itest);
  for (;i-->0;test++) {
    if (akfceu_itest_should_run(test,argc,argv)) {
      if (test->fn()<0) {
        AKFCEU_TEST_REPORT_FAIL("%s [%s:%d]",test->name,test->file,test->line)
      } else {
        AKFCEU_TEST_REPORT_PASS("%s [%s:%d]",test->name,test->file,test->line)
      }
    } else {
      AKFCEU_TEST_REPORT_SKIP("%s [%s:%d]",test->name,test->file,test->line)
    }
  }
  return 0;
}
