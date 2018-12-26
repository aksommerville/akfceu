#ifndef AKFCEU_VM_INTERNAL_H
#define AKFCEU_VM_INTERNAL_H

#include "akfceu.h"
#include "akfceu_vm.h"

/* Globals.
 *****************************************************************************/

struct akfceu_vm {

  /* CPU. */
  struct {
    uint16_t pc;
    uint8_t a,x,y,s,p;
  } cpu;

};

#endif
