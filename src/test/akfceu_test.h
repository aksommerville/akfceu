#ifndef AKFCEU_TEST_H
#define AKFCEU_TEST_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

/* Support for integration tests.
 ***********************************************************/
 
#define AKFCEU_TEST(name,...) int name()
#define XXX_AKFCEU_TEST(name,...) int name()

/* Support for unit tests.
 ************************************************************/
 
static inline int akfceu_test_matches_argv(const char *tname,int argc,char **argv) {
  if (argc<2) return 1;
  int i=1; for (;i<argc;i++) {
    const char *arg=argv[i];
    if (!strcmp(arg,tname)) return 1;
  }
  return 0;
}
 
#define AKFCEU_UNIT_TEST(fnname) { \
  if (!akfceu_test_matches_argv(#fnname,argc,argv)) { \
    AKFCEU_TEST_REPORT_SKIP("%s [%s:%d]",#fnname,__FILE__,__LINE__) \
  } else if (fnname()<0) { \
    AKFCEU_TEST_REPORT_FAIL("%s [%s:%d]",#fnname,__FILE__,__LINE__) \
  } else { \
    AKFCEU_TEST_REPORT_PASS("%s [%s:%d]",#fnname,__FILE__,__LINE__) \
  } \
}

/* Communication with the test runner.
 ************************************************************/
 
#define AKFCEU_TEST_REPORT(fmt,...) printf("AKFCEU_TEST "fmt"\n",##__VA_ARGS__);
#define AKFCEU_TEST_REPORT_PASS(fmt,...) AKFCEU_TEST_REPORT("PASS "fmt,##__VA_ARGS__)
#define AKFCEU_TEST_REPORT_FAIL(fmt,...) AKFCEU_TEST_REPORT("FAIL "fmt,##__VA_ARGS__)
#define AKFCEU_TEST_REPORT_SKIP(fmt,...) AKFCEU_TEST_REPORT("SKIP "fmt,##__VA_ARGS__)
#define AKFCEU_TEST_REPORT_DETAIL(fmt,...) AKFCEU_TEST_REPORT("DETAIL "fmt,##__VA_ARGS__)

#define AKFCEU_FAIL_MORE(key,fmt,...) AKFCEU_TEST_REPORT_DETAIL("| %15s: "fmt,key,##__VA_ARGS__)
#define AKFCEU_FAIL_END { \
  AKFCEU_TEST_REPORT_DETAIL("+--------------------------------------------") \
  return -1; \
}
#define AKFCEU_FAIL_BEGIN(fmt,...) { \
  AKFCEU_TEST_REPORT_DETAIL("+--------------------------------------------") \
  AKFCEU_TEST_REPORT_DETAIL("| ASSERTION FAILED") \
  AKFCEU_FAIL_MORE("Location","%s:%d (%s)",__FILE__,__LINE__,__func__) \
  if (fmt[0]) AKFCEU_FAIL_MORE("Message",fmt,##__VA_ARGS__) \
}

/* Assertions.
 ***************************************************************/
 
#define AKFCEU_ASSERT(condition,...) { \
  if (!(condition)) { \
    AKFCEU_FAIL_BEGIN(""__VA_ARGS__) \
    AKFCEU_FAIL_MORE("Expected","true") \
    AKFCEU_FAIL_MORE("As written","%s",#condition) \
    AKFCEU_FAIL_END \
  } \
}
 
#define AKFCEU_ASSERT_NOT(condition,...) { \
  if (condition) { \
    AKFCEU_FAIL_BEGIN(""__VA_ARGS__) \
    AKFCEU_FAIL_MORE("Expected","false") \
    AKFCEU_FAIL_MORE("As written","%s",#condition) \
    AKFCEU_FAIL_END \
  } \
}

#define AKFCEU_ASSERT_CALL(call,...) { \
  int _result=(call); \
  if (_result<0) { \
    AKFCEU_FAIL_BEGIN(""__VA_ARGS__) \
    AKFCEU_FAIL_MORE("Expected","successful call") \
    AKFCEU_FAIL_MORE("As written","%s",#call) \
    AKFCEU_FAIL_MORE("Value","%d",_result) \
    AKFCEU_FAIL_END \
  } \
}

#define AKFCEU_ASSERT_FAILURE(call,...) { \
  int _result=(call); \
  if (_result>=0) { \
    AKFCEU_FAIL_BEGIN(""__VA_ARGS__) \
    AKFCEU_FAIL_MORE("Expected","failed call") \
    AKFCEU_FAIL_MORE("As written","%s",#call) \
    AKFCEU_FAIL_MORE("Value","%d",_result) \
    AKFCEU_FAIL_END \
  } \
}

#define AKFCEU_ASSERT_INTS_OP(a,op,b,...) { \
  int _a=(a),_b=(b); \
  if (!(_a op _b)) { \
    AKFCEU_FAIL_BEGIN(""__VA_ARGS__) \
    AKFCEU_FAIL_MORE("As written","%s %s %s",#a,#op,#b) \
    AKFCEU_FAIL_MORE("Value","%d %s %d",_a,#op,_b) \
    AKFCEU_FAIL_END \
  } \
}

#define AKFCEU_ASSERT_INTS(a,b,...) AKFCEU_ASSERT_INTS_OP(a,==,b,##__VA_ARGS__)

#define AKFCEU_ASSERT_STRINGS(a,ac,b,bc,...) { \
  const char *_a=(a),*_b=(b); \
  int _ac=(ac),_bc=(bc); \
  if (!_a) _ac=0; else if (_ac<0) { _ac=0; while (_a[_ac]) _ac++; } \
  if (!_b) _bc=0; else if (_bc<0) { _bc=0; while (_b[_bc]) _bc++; } \
  if ((_ac!=_bc)||memcmp(_a,_b,_ac)) { \
    AKFCEU_FAIL_BEGIN(""__VA_ARGS__) \
    AKFCEU_FAIL_MORE("(A) As written","%s : %s",#a,#ac) \
    AKFCEU_FAIL_MORE("(B) As written","%s : %s",#b,#bc) \
    AKFCEU_FAIL_MORE("(A) Value","%.*s",_ac,_a) \
    AKFCEU_FAIL_MORE("(B) Value","%.*s",_bc,_b) \
    AKFCEU_FAIL_END \
  } \
}

#define AKFCEU_ASSERT_FLOATS(a,b,e,...) { \
  double _a=(a),_b=(b),_e=(e); \
  double d=_a-_b; if (d<0.0) d=-d; \
  if (d>_e) { \
    AKFCEU_FAIL_BEGIN(""__VA_ARGS__) \
    AKFCEU_FAIL_MORE("As written","%s == %s within %s",#a,#b,#e) \
    AKFCEU_FAIL_MORE("Values","%f == %f",_a,_b) \
    AKFCEU_FAIL_MORE("Difference","%f",d) \
    AKFCEU_FAIL_END \
  } \
}

#endif
