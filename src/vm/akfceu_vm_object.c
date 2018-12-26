#include "akfceu_vm_internal.h"

/* Allocation.
 */

struct akfceu_vm *akfceu_vm_new() {
  struct akfceu_vm *vm=calloc(1,sizeof(struct akfceu_vm));
  if (!vm) return 0;
  return vm;
}

void akfceu_vm_del(struct akfceu_vm *vm) {
  if (!vm) return;
  free(vm);
}
