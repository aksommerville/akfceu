#ifndef AKFCEU_TEST_H
#define AKFCEU_TEST_H

#include <stdio.h>

#define AKFCEU_UNIT_TEST(name) int name()
#define XXX_AKFCEU_UNIT_TEST(name) int name()

#define AKFCEU_ASSERT(condition,fmt,...) if (!(condition)) { \
  printf("Boolean assertion failed at %s:%d.\n",__FILE__,__LINE__); \
  printf("As written: %s\n",#condition); \
  return -1; \
}
#define AKFCEU_ASSERT_NOT(condition,fmt,...) if (condition) { \
  printf("Negative assertion failed at %s:%d.\n",__FILE__,__LINE__); \
  printf("As written: %s\n",#condition); \
  return -1; \
}

#endif
