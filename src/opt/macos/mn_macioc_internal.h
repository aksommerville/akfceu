#ifndef MN_MACIOC_INTERNAL_H
#define MN_MACIOC_INTERNAL_H

#include "mn_macioc.h"
#include "util/mn_time.h"
#include <Cocoa/Cocoa.h>

#define MN_FRAME_RATE 60

@interface AKAppDelegate : NSObject <NSApplicationDelegate> {
}
@end

extern struct mn_macioc {
  int init;
  struct mn_ioc_delegate delegate;
  int terminate;
  int focus;
  int update_in_progress;
  const char *rompath;
} mn_macioc;

void mn_macioc_abort(const char *fmt,...);
int mn_macioc_call_init();
void mn_macioc_call_quit();

#endif
