#include "test/akfceu_test.h"
#include "vm/akfceu_vm_internal.h"

/* Test allocation and basic properties of a VM.
 */

AKFCEU_UNIT_TEST(akfceu_test_vm_object) {
  struct akfceu_vm *vm=akfceu_vm_new();
  AKFCEU_ASSERT(vm,"akfceu_vm_new()")

  akfceu_vm_del(vm);
  return 0;
}
