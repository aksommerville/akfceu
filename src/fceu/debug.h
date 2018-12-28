/* aks: This is fucking cool. How can we use it?
 */

#ifndef FCEU_DEBUG_H
#define FCEU_DEBUG_H

void FCEUI_DumpMem(const char *fname, uint32_t start, uint32_t end);
void FCEUI_LoadMem(const char *fname, uint32_t start, int hl);

#ifdef FCEUDEF_DEBUGGER

/* Type attributes, you can OR them together. */
#define BPOINT_READ    1
#define BPOINT_WRITE    2
#define BPOINT_PC    4

#include "x6502struct.h"

void FCEUI_SetCPUCallback(void (*callb)(X6502 *X));

int FCEUI_DeleteBreakPoint(uint32_t w);

int FCEUI_ListBreakPoints(
  int (*callb)(int type, unsigned int A1, unsigned int A2, void (*Handler)(X6502 *, int type, unsigned int A) )
);

int FCEUI_GetBreakPoint(
  uint32_t w, int *type, unsigned int *A1, unsigned int *A2,
  void (**Handler)(X6502 *, int type, unsigned int A)
);

int FCEUI_SetBreakPoint(
  uint32_t w, int type, unsigned int A1, unsigned int A2,
  void (*Handler)(X6502 *, int type, unsigned int A)
);

int FCEUI_AddBreakPoint(
  int type, unsigned int A1, unsigned int A2,
  void (*Handler)(X6502 *, int type, unsigned int A)
);

#endif

#endif
